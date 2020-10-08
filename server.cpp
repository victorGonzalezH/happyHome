/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 03/17/2013 : Charles-Henri Hallard (http://hallard.me)
              Modified to use with Arduipi board http://hallard.me/arduipi
						  Changed to use modified bcm2835 and RF24 library
TMRh20 2014 - Updated to work with optimized RF24 Arduino library

 */

/**
 * Example RF Radio Ping Pair
 *
 * This is an example of how to use the RF24 class on RPi, communicating to an Arduino running
 * the GettingStarted sketch.
 */



#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <fstream>
#include <list>
#include <algorithm>
#include <RF24/RF24.h>

using namespace std;

class Address
{

    public:
    

    unsigned char address[5];

    bool isAssigned;

    Address()
    {
        isAssigned = false;
    }

    Address(unsigned char * addressParam)
    {
        isAssigned = false;
        for(int i = 0; i < 5; i++)
        {
            address[i] = addressParam[i];
        }
    }


    Address(unsigned char * addressParam, bool isAssigned)
    {
        this->isAssigned = isAssigned;
        for(int i = 0; i < 5; i++)
        {
            address[i] = addressParam[i];
        }
    }

    
    //Imprime la direccion
    //0 Decimal
    //1 Hexadecimal
    //2 Caracter
    void PrintAddress(int format)
    {
        char formatTemplate[3] = "%d";
        switch (format)
        {
            default:
            case 0:
                formatTemplate[1] = 'd';
                break;
            
            case 1:
                formatTemplate[1] = 'x';
                break;

            case 2:
                formatTemplate[1] = 'c';
                break;
        }

        for(int i = 0; i < 5; i++)
        {
            printf(formatTemplate, address[i]);
        }
    }

};


class RFNode 
{
    public:
        unsigned char nodeId;
        unsigned char * address;

    //Imprime la direccion
    //0 Decimal
    //1 Hexadecimal
    //2 Caracter
    void PrintAddress(int format)
    {
        char formatTemplate[3] = "%d";
        switch (format)
        {
            default:
            case 0:
                formatTemplate[1] = 'd';
                break;
            
            case 1:
                formatTemplate[1] = 'x';
                break;

            case 2:
                formatTemplate[1] = 'c';
                break;
        }

        for(int i = 0; i < 5; i++)
        {
            printf(formatTemplate, address[i]);
        }
    }
};



class Node
{
    public:
    RFNode rfNode;
    Node    * childs;
    Node    *  next;

    Node(unsigned char nodeId, unsigned char * address)
    {
        rfNode.nodeId = nodeId;
        rfNode.address = address;
    }


    //Agrega un nuevo nodo
    Node * AddChild(unsigned char childId, unsigned char * childAddress)
    {
        Node * child = new Node(childId, childAddress);

        // No existe ningun nodo hijo
        if(childs == NULL)
        {
            this->childs = child;
        }
        else 
        {
            //Ya existen hijos agregados, las busqueda es secuencial, no es la mejor opcion pero
            // sin embargo para el numero de nodos se puede permitir
            Node * search = this->childs;
            while (search != NULL && search->next != NULL)
            {
                search = search->next;
            }
            
            search->next = child;
        }
        
        return child;
    }


    std::list<Address> GetChildsAddresses()
    {

        // No existe ningun nodo hijo
        if(childs == NULL)
        {
            return NULL;
        }
        else
        {
            Node * search = this->childs;
            while (search != NULL && search->next != NULL)
            {
                search = search->next;
            }
            
            search->next = child;

        }
    }
};


////////////////////////////////////////////////////////////////////////////////////
// Declaracion de variables globales
//Estructura  en modo normal para el envio/recepcion de datos
struct dataStruct
{
  unsigned char toNodeId;       //Indica el nodo al que va dirigido el mensaje
  unsigned char fromNodeId;     //Indica el nodo de quien viene el mensaje
  unsigned char operationType;  //Indica el tipo de operacion
  unsigned char floatValue[6];           // Es usado para enviar/recibir un valor de punto flotante, generalmente valores de sensores
  bool  onOffValue;             // Es usado para enviar/recibir un valor de tipo booleano, generalemnte para valores de encendido/apagado
  char  value;                  // Es usado para enviar/recibir valor de tipo entero/caracter, generalmente para valores enteros
  
} dataForSend, dataForReceive;


//Estructura para la activacion de nodos
struct syncStruct
{
   unsigned char tempNodeId[13];
   unsigned char address[5];
   unsigned char address2[5];
   unsigned char operationType;
   unsigned char fatherId;
   
} syncDataForSend, syncDataForReceive;


// Raiz del arbol de la red de nodos
Node * root;

// Lista de las direcciones usadas por el servidor
std::list<Address> addresses;

//Identificador usado para hacer broadcast a los nodos.
const unsigned char NODEID_FOR_BROADCAST = 255;

//Operaciones para los nodos RF
const unsigned char RF_JUST_DETECTING_OPERATION     = 0;
const unsigned char RF_REQUEST_BROADCAST_MODE       = 1;
const unsigned char RF_REQUEST_ADDRESS              = 2;
const unsigned char RF_RESPONSE_ADDRESS             = 3;

//Modo
uint8_t mode; //Indica el modo en que se encuentra el servidor

//Modos en los que se encuentra el servidor de RFs
const uint8_t NORMAL_MODE       = 0;
const uint8_t BROADCAST_MODE    = 1;

//Commands desde el servidor web
const uint8_t ENABLE_NORMAL_MODE    = 0;
const uint8_t ENABLE_BROADCAST_MODE = 1;

unsigned char tempNodeIdDefaultValue[] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
unsigned char defaultFloatValue[] = {'0','0','0','.','0','0'};
////////////////////////////////////////////////////////////////////////////////////

// Configuracion del hardware de radio para raspberry pi
// Setup for GPIO 15 CE and CE0 CSN with SPI Speed @ 8Mhz
//RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);
RF24 radio(22,0);



/*********************************************************************************************************************/
//Compara dos arreglos de caracteres. El parametro length indica la longitud de ambos arreglos
//a arreglo 1
//b arreglo 2
//length longitud de los arreglos
bool CompareCharArray(unsigned char * a, unsigned char * b, int length)
{

    int i = 0;
    while(a[i] == b[i])
    {
        i++;
    }

    return i < length? true: false;
}


void CopyCharArray(unsigned char * source, unsigned char * dest, int length)
{
    for(int i = 0; i < length; i++)
    {
        dest[i] = source[i];
    }
}


std::list<Address> GetAddreses(string addressesFileName)
{

  std::list<Address> addresses;
  
  string line;
  ifstream addressesFile (addressesFileName);
  if (addressesFile.is_open())
  {
    
    while ( getline (addressesFile,line) )
    {
        bool isAssigned = false;
        unsigned char * address = new unsigned char[5];
        int columnIndex = 0;
        int initIndex   = 0;
        int finalIndex  = 0;
        while(columnIndex < 2)
        {
            finalIndex = line.find(' ',initIndex);
            string splitedLine = line.substr(initIndex, finalIndex - initIndex);

            if(columnIndex == 0)
            {

                int j = 0;
                int addressInitIndex = 0;
                int addressFinalIndex = 0;

                while(j < 5)
                {
                    addressFinalIndex = splitedLine.find(',', addressInitIndex);
                    string addressSplitedLine = splitedLine.substr(addressInitIndex, addressFinalIndex - addressInitIndex);
                    address[j] = (unsigned char)std::stoi (addressSplitedLine,nullptr,16);
                    addressInitIndex = addressFinalIndex + 1;
                    j++;
                }
                
            }
            else if(columnIndex == 1)
            {
                if(splitedLine.compare("1") == 0)
                {
                    isAssigned = true;
                } 
            }
            
            columnIndex++;
            initIndex = finalIndex + 1;
        }


        // Se inserta la direccion
        addresses.push_back(Address(address, isAssigned));
    }

        addressesFile.close();
  }
  else 
  {
      cout << "Unable to open file";
  }
 
    return addresses;
}

//Obtiene un nodo de acuerdo a su id
Node * GetNode(Node * root, unsigned char nodeId)
{
    if(root != NULL)
    {
        if(root->rfNode.nodeId == nodeId)
        {
            return root;
        }
        else
        {
            
            Node * currentNode = root->childs;
            if(currentNode != NULL)
            {
                Node * result = GetNode(currentNode, nodeId);
                if(result == NULL)
                {
                    currentNode = currentNode->next;    
                }
                else
                {
                    return result;
                }
                
            }

            return NULL;
            
        }
        
    }

    return NULL;
    
}


//Construye el arbol de los nodos. El archivo de entrada debera de cumplir el formato:
//FatherNodeId     NodeId    Address
    //-1           0         0x00,0x00,0x00,0x00,0x00
    // 0           1         0x00,0x00,0x00,0x00,0x01
Node * BuildTree(string fileName, int * nodesNumber)
{

  string line;
  ifstream nodesFileName (fileName);
  Node * root = NULL;
  if (nodesFileName.is_open())
  {
    
    while ( getline (nodesFileName,line) )
    {
        //std::cout << line << '\n';
        unsigned char nodeId   = 0;
        unsigned char fatherId = 0;
        int initIndex   = 0;
        int finalIndex  = 0;
        int columnIndex = 0;
        unsigned char * address = new unsigned char[5];
        while(columnIndex < 3)
        {
            finalIndex = line.find(' ',initIndex);
            //std::cout << finalIndex << '\n';
            string splitedLine = line.substr(initIndex, finalIndex - initIndex);
            //std::cout << "splitedLine " << splitedLine << '\n';
            //Se lee la columna del id nodo padre
            if(columnIndex == 0)
            {
                std::string::size_type sz;
                fatherId = std::stoi (splitedLine,&sz);
                //std::cout << "fatherId: ";
                //printf("%d\n", fatherId);
                
            }
            else if(columnIndex == 1)
            {
                std::string::size_type sz;
                nodeId = std::stoi (splitedLine,&sz);
                //std::cout << "nodeId: ";
                //printf("%d\n", fatherId);
            }
            else if(columnIndex == 2)
            {   
                int j = 0;
                int addressInitIndex = 0;
                int addressFinalIndex = 0;

                while(j < 5)
                {
                    addressFinalIndex = splitedLine.find(',', addressInitIndex);
                    string addressSplitedLine = splitedLine.substr(addressInitIndex, addressFinalIndex - addressInitIndex);
                    address[j] = (unsigned char)std::stoi (addressSplitedLine,nullptr,16);
                    addressInitIndex = addressFinalIndex + 1;
                    //printf("Address: %x\n", address[j]);
                    j++;
                }
            }
            
            columnIndex++;
            initIndex = finalIndex + 1;
        }


        if(nodeId == 0)
        {
            root = new Node(nodeId, address);
        }
        else
        {
            Node * fatherNode = GetNode(root, fatherId);
            if(fatherNode != NULL)
            {
                fatherNode->AddChild(nodeId, address); 
            }
            else
            {
                //ERROR
            }
            
        }

        (*nodesNumber)++;
        
    }

        nodesFileName.close();
  }

  else 
  {
      cout << "Unable to open file";
  }

    return root;
}



void PrintTree(Node * root, int format)
{
    if(root != NULL)
    {
        printf("%d, ", root->rfNode.nodeId);
        root->rfNode.PrintAddress(format);
        printf("\n");
        if(root->childs != NULL)
        {
            Node * child = root->childs;
            while (child != NULL)
            {
                PrintTree(child, format);
                child = child->next;
            }
            
        }
    }
}

//Obtiene una direccion libre
Address GetAvailableAddress()
{
    list<Address>::iterator it = std::find_if(addresses.begin(), addresses.end(), [] (const Address& a) { return a.isAssigned == true; });
    return *it;
}


//
Address GetDefaultWriteAddress()
{
    //                    0       1       2       3       4
    //uint8_t address[5] = { 0x00, 0x00, 0x00, 0x00, 0x01 };
    uint8_t address[] = { "1Node" };
    Address a (address);
    return a;
}

//
Address GetDefaultReadAddress()
{
    //                    0       1       2       3       4
    //uint8_t address[5] = { 0x01, 0x00, 0x00, 0x00, 0x02 };
    uint8_t address[] = { "2Node" };
    Address a (address);
    return a;
}


///
// Establece los valores a una estrutura de tipo dataStruct
//
dataStruct setDataForSend(unsigned char fromNodeId, unsigned char toNodeId, unsigned char operationType, char value, unsigned char * floatValue, bool onOffValue)
{
    dataStruct data;
    data.toNodeId       = toNodeId;
    data.operationType  = operationType;
    data.fromNodeId     = fromNodeId;
    data.value          = value;
    data.onOffValue     = onOffValue;
    if(floatValue != NULL)
    {
        CopyCharArray(floatValue, data.floatValue, 6);
    }
    else
    {
        unsigned char defaultValue[] = { '0', '0', '0', '0', '0', '0'};
        CopyCharArray(defaultValue, data.floatValue, 6);
    }
    
    
    return data;
}




bool sendData(dataStruct data)
{
    return radio.write(&dataForSend, sizeof(dataForSend));
}



//
// Activa el modo "agregar nodo" en el nodo toNodeId para que un nodo cercano a el pueda enviarle mensaje de solicitud
// para agregarse a la red
//
// bool enableAddNodeMode(unsigned char toNodeId)
// {
//     dataStruct data = setData(0, toNodeId, 1);
//     return sendData(data);
// }


void setupRadio(uint8_t readAddress[], uint8_t writeAddress[], size_t payloadSize, uint8_t channel, rf24_datarate_e dataRate)
{
    //Se configura y se inicializa el radio
    radio.begin();

    radio.setRetries(15, 15);

    radio.setPayloadSize(payloadSize);
    
    radio.setCRCLength(RF24_CRC_8);
    
    radio.setPALevel(RF24_PA_MAX);
    
    radio.setDataRate(dataRate);

    radio.setChannel(channel);

    radio.openWritingPipe(writeAddress);

    radio.openReadingPipe(1, readAddress);

    radio.printDetails();
}

void activeNode(size_t payloadSize, unsigned long maxtime)
{
    //Operacion tipo 0 indica ninguna operacion por hacer
    syncDataForSend.operationType = 0;
    
    unsigned char tempNodeId[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
    unsigned long initProcess = millis();
    while(millis() - initProcess < maxtime)
    {

        radio.stopListening();
        radio.write(&syncDataForSend, payloadSize);
        radio.startListening();        
        unsigned long init = millis();
        bool timeout = false;
        // Busca el datoFetch the payload, and see if this was the last one.
        while (!radio.available() && !timeout) 
        {
                if (millis() - init > 300) 
                {
                    timeout = true;
                }

        }

        if(!timeout)
        {
             radio.read(&syncDataForReceive, payloadSize);
             printf("Se recibe valor: (%d)...\n", syncDataForReceive.operationType);
             //Si el tempNodeId tiene el valor por defecto, entonces es la primera vez que se accede a el metodo
             // entonces se copia  el valor del nodeid temporal que esta solicitando una direccion.
             if(CompareCharArray(tempNodeId, tempNodeIdDefaultValue, 13))
             {
                    CopyCharArray(syncDataForReceive.tempNodeId, tempNodeId, 13);
             }
             //Con esto nos aseguramos que los mensajes que se reciban solo se procecen aquellos del nodo
             //que solicito (del quien se recibio mensaje primero)
             if(CompareCharArray(syncDataForReceive.tempNodeId, tempNodeId, 13))
             {
                
                //Si el nodo esta solicitando una direccion
                if(syncDataForReceive.operationType == RF_REQUEST_ADDRESS)
                {
                    // Se obtiene una direccion disponible, esta direccion sera la direccion de escritura
                    //del nodo
                    Address writeAddress = GetAvailableAddress();

                    // Cuando el valor de  value es igual a cero, significa que este mensaje no ha sido retransmitido
                    //por lo tanto este nodo esta cerca del nodo cero (servidor)
                    if(syncDataForReceive.fatherId ==  0)
                    {
                        //La direccion de escritura obtenida se guarda como un nodo en las direcciones de lectura
                        //del nodo del servidor
                        ////

                        syncDataForSend.fatherId        = 0;
                        // Se establece la operacion de respuesta de direccion
                        syncDataForSend.operationType   = RF_RESPONSE_ADDRESS;
                        CopyCharArray(syncDataForReceive.tempNodeId,syncDataForSend.tempNodeId, 13);
                        //Se copia la direccion de escritural del nodo
                        CopyCharArray(writeAddress.address, syncDataForSend.address, 5);

                        CopyCharArray(root->rfNode.address, syncDataForSend.address2, 5);
                    }
                    else
                    {
                        
                    }
                }
             }
        }

        
        //Retraso de 100 milisegundos
        delay(50);
        
    }

}

void PerformOperation(dataStruct data)
{

}


void normalOperation(unsigned int delayTime)
{
    radio.stopListening();
    radio.write(&dataForSend, sizeof(dataStruct));            
    radio.startListening();        
    unsigned long init = millis();
    bool timeout = false;
    
    while (!radio.available() && !timeout) 
    {
        if (millis() - init > 300) 
        {
            timeout = true;
        }

    }
    
    if(timeout)
    {
        
    }
    else
    {
        radio.read(&dataForReceive, sizeof(dataStruct));
        PerformOperation(dataForReceive);
        printf("Se recibe valor: (%d)...\n", dataForReceive.value);
    }
    
    delay(delayTime); //Delay after payload responded to, minimize RPi CPU time
}

void normalOperationWithTimeout(unsigned int delayTime, unsigned long timeout)
{

    unsigned long start = millis();
    while(millis() - start < timeout)
    {
        normalOperation(delayTime);
    }
}


//Direcciona los comandos hacia la tarea que se debe ejecutar de acuerdo a el estado en que se encuetra el servidor
void AddressCommand(uint8_t command)
{
    switch (command)
    {
        case ENABLE_BROADCAST_MODE:
            switch (mode)
            {
                case BROADCAST_MODE:
                case NORMAL_MODE:
                    cout << "Activate node mode on\n";
                    setupRadio(GetDefaultReadAddress().address, GetDefaultWriteAddress().address, sizeof(syncStruct), 120, RF24_250KBPS);
                    activeNode(sizeof(syncStruct), 5000);
                    cout << "Activate node mode off\n";
                    cout << "Activate normal mode on\n";
                    setupRadio(GetDefaultReadAddress().address, GetDefaultWriteAddress().address, sizeof(dataStruct), 120, RF24_250KBPS);
                    normalOperationWithTimeout(50, 5000);
                    mode = BROADCAST_MODE;
                    cout << "Activate normal mode off\n";
                break;
        
            default:
                break;
            }
            
            break;
            
        case ENABLE_NORMAL_MODE:
            switch (mode)
            {
                case BROADCAST_MODE:
                    setupRadio(GetDefaultReadAddress().address, GetDefaultWriteAddress().address, sizeof(dataStruct), 120, RF24_250KBPS);
                    mode = NORMAL_MODE;
                case NORMAL_MODE:
                    setDataForSend(root->rfNode.nodeId, NODEID_FOR_BROADCAST, RF_JUST_DETECTING_OPERATION, 0, defaultFloatValue , false);
                    normalOperation(50);
                    mode = NORMAL_MODE;
                break;
            
            default:
                break;
            }
        break;

        default:
            break;
    }
}





// Imprime las direcciones
void printAdresses()
{
    std::list<Address>::iterator iter;

    for(iter = addresses.begin(); iter != addresses.end(); ++iter)
    {
        Address address = *iter;
        address.PrintAddress(0);
        std::cout << " " << address.isAssigned;
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
}



// Programa principal
int main(int argc, char** argv)
{
    cout << "Server init\n";
    mode = NORMAL_MODE;
    // Se obtienen las direcciones que se usaran 
    addresses = GetAddreses("addresses.txt");
    //printAdresses();
    
    int nodesCount = 0;
    root = BuildTree("nodes.txt", &nodesCount);
    
    // Por defecto, el servidor se establece en modo normal operativo
    uint8_t command = ENABLE_NORMAL_MODE;
    // Se configura el radio en modo normal
    setupRadio(GetDefaultReadAddress().address, GetDefaultWriteAddress().address, sizeof(dataStruct), 120, RF24_250KBPS);
    
    while(true)
    {
        // Direcciona el comando recibido
        AddressCommand(command);
    }
    
    return 0;
}


    // if(argc == 2)
    // {
    //     string mode = argv[1];
    //     if(mode.compare("0") == 0) //Se activa en modo activar nodos
    //     {
           

    //     }
    //     else if(mode.compare("1") == 0) //Se activa modo normal
    //     {
    //         // Se configura el radio para operar en modo normal
    //         setupRadio(GetDefaultReadAddress().address, GetDefaultWriteAddress().address, sizeof(dataStruct), 120, RF24_250KBPS);
    //         setDataForSend(root->rfNode.nodeId, nodeIdForBroadcast, JUST_DETECTING_OPERATION, 0, defaultFloatValue , false);
    //         while (true) 
    //         {

    //                 radio.stopListening();
    //                 radio.write(&dataForSend, sizeof(dataStruct));            
    //                 radio.startListening();        
    //                 unsigned long init = millis();
    //                 bool timeout = false;
                    
    //                 while (!radio.available() && !timeout) 
    //                 {
    //                     if (millis() - init > 300) 
    //                     {
    //                         timeout = true;
    //                     }

    //                 }
                    
    //                 if(timeout)
    //                 {
                        
    //                 }
    //                 else
    //                 {
    //                     radio.read(&dataForReceive, sizeof(dataStruct));
    //                     PerformOperation(dataForReceive);
    //                     printf("Se recibe valor: (%d)...\n", dataForReceive.value);
    //                 }
                    
    //                 delay(100) //Delay after payload responded to, minimize RPi CPU time

    //         }
    //     }
    // }