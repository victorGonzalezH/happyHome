/*
 Happy Home radio Server V1

 This is server es based on the TMRh20 Library from TMRh and collaborators.

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



class Address
{

    public:
    

    unsigned char *address;

    bool isAssigned;

    Address()
    {
        isAssigned = false;
        address = new unsigned char[5];
    }

    Address(unsigned char * addressParam)
    {
        isAssigned = false;
        address = new unsigned char[5];
        for(int i = 0; i < 5; i++)
        {
            address[i] = addressParam[i];
        }
    }


    Address(unsigned char * addressParam, bool isAssigned)
    {
        this->isAssigned = isAssigned;
        address = new unsigned char[5];
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
    int childsCounter;
    
    public:
    RFNode rfNode;
    Node    *childs;
    Node    *next;
    Node    *father;

    Node(unsigned char nodeId, unsigned char * address)
    {
        rfNode.nodeId = nodeId;
        rfNode.address = address;
        rfNode.PrintAddress(0);
        childsCounter = 0;
        childs = NULL;
        next = NULL;
        father = NULL;
    }


    //Agrega un nuevo nodo
    Node * AddChild(unsigned char childId, unsigned char * childAddress)
    {
        Node * child = new Node(childId, childAddress);
        father = this;
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

        childsCounter++;
        
       
        return child;
    }

    int GetChildsCount(){ return childsCounter; }

    unsigned char GetFatherId() { return father->rfNode.nodeId; }

    //Obtiene las direcciones de los nodos hijos
    unsigned char ** GetChildsAddresses()
    {
        std::list<Address> addresses;
        unsigned char **addressesAsCharArray = NULL;
        // No existe ningun nodo hijo
        if(childs != NULL)
        {
            Node * search = this->childs;
            while (search != NULL)
            {
                // Se inserta la direccion
                addresses.push_back(Address(search->rfNode.address, true));

                search = search->next;
            }

            addressesAsCharArray = new unsigned char*[addresses.size()];
            int index = 0;
            for (std::list<Address>::iterator it = addresses.begin(); it != addresses.end(); ++it)
            {
                addressesAsCharArray[index] = new unsigned char[5];
                CopyCharArray(it->address, addressesAsCharArray[index], 5);
                index++;
            }

            
        }
        // int len = addresses.size();
        // cout << "Imprimiendo las direcciones: " << len << "\n";
        // for(int i = 0; i < len; i++)
        // {
        //     for(int j = 0; j < 5; j++)
        //     {
        //         printf("%d",addressesAsCharArray[i][j]); 
        //     }
        //         cout<<"\n";
        // }

        return addressesAsCharArray;
    }
};


////////////////////////////////////////////////////////////////////////////////////
// Declaracion es estructuras
//Estructura  en modo normal para el envio/recepcion de datos
struct dataStruct
{
  uint8_t toNodeId;       //Indica el nodo al que va dirigido el mensaje
  uint8_t fromNodeId;     //Indica el nodo de quien viene el mensaje
  uint8_t operationType;  //Indica el tipo de operacion
  uint8_t floatValue[5];           // Es usado para enviar/recibir un valor de punto flotante, generalmente valores de sensores
  bool  onOffValue;             // Es usado para enviar/recibir un valor de tipo booleano, generalemnte para valores de encendido/apagado
  uint8_t  value;                  // Es usado para enviar/recibir valor de tipo entero/caracter, generalmente para valores enteros
  
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

//////////////////////////////////////////////////////////////////////////////////////
// Declaracion de variables globales
// Raiz del arbol de la red de nodos
Node * root;

// Lista de las direcciones usadas por el servidor
std::list<Address> addresses;

// Indica el numero de nodos que estan conectados
unsigned char nodesCount = 0;

// Indica el modo en que se encuentra el servidor
uint8_t mode;

// Id temporal del nodo que se esta activando
unsigned char tempNodeIdForActivate[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };

// Nodo que fue activado y que esta pendiente por confirmar su activacion. Cuando el nodo se activa guarda este este nodo temporal y empieza a sensar normalmente, por eso cuando se recibe
// un mensaje cuando el servidor esta en modo activacion se debe de verificar si el mensaje proviene de este nodo, si es asi se debe de notificar
// a el servidor web para que detenga el proceso de activacion en caso de que aun no se haya terminado
Node *nodeActivationPending = NULL;

unsigned char fatherIdOfTheNodeActivationPending = 0;

// Nombre del archivo que contiene las direcciones
char addressesFileName[] = {"addresses.txt"};

// Nombre del archivo que contiene el registro de los nodos
char nodesFileName[] = {"nodes.txt"};

//Por defecto, el servidor se establece en modo normal operativo
uint8_t command;

//////////////////////////////////////////////////////////////////////////////////////
// Declaracion de constantes
//Identificador usado para hacer broadcast a los nodos.
const unsigned char NODEID_FOR_BROADCAST = 255;

//Operaciones para los nodos RF
const unsigned char RF_SENSING                       = 0;
const unsigned char RF_REQUEST_ADDRESS               = 1;
const unsigned char RF_RESPONSE_ADDRESS              = 2;
const unsigned char RF_RESPONSE_ADDRESS_RECEIVED     = 3;
const unsigned char RF_REQUEST_BROADCAST_MODE        = 4;
const unsigned char RF_RESPONSE_BROADCAST_MODE       = 5;


//Modos en los que se encuentra el servidor de RFs
const uint8_t NORMAL_MODE      = 0;
const uint8_t ACTIVATE_MODE    = 1;

//Commands desde el servidor web
const uint8_t ENABLE_NORMAL_MODE    = 0;
const uint8_t ENABLE_BROADCAST_MODE = 1;

unsigned char tempNodeIdDefaultValue[]  = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
unsigned char defaultFloatValue[]       = {'0','0','0','.','0','0'};
////////////////////////////////////////////////////////////////////////////////////

// Configuracion del hardware de radio para raspberry pi
// Setup for GPIO 15 CE and CE0 CSN with SPI Speed @ 8Mhz
//RF24 radio(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_24, BCM2835_SPI_SPEED_8MHZ);
RF24 radio(22,0);



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
                    //address[j] = (unsigned char)std::stoi (addressSplitedLine,nullptr,10);
                    address[j] = (unsigned char)addressSplitedLine[0];
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
Node * BuildTree(string fileName)
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
                    //address[j] = (unsigned char)std::stoi (addressSplitedLine,nullptr,16);
                    address[j] = (unsigned char) addressSplitedLine[0];
                    addressInitIndex = addressFinalIndex + 1;
                    printf("Address: %x\n", address[j]);
                    j++;
                }
            }
            
            columnIndex++;
            initIndex = finalIndex + 1;
        }

        printf("NodeId: %d\n", nodeId);

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

        nodesCount++;
        
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
    list<Address>::iterator it = std::find_if(addresses.begin(), addresses.end(), [] (const Address& a) { return a.isAssigned == false; });
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
void setDataForSend(unsigned char fromNodeId, unsigned char toNodeId, unsigned char operationType, char value, unsigned char * floatValue, bool onOffValue)
{
    
    dataForSend.toNodeId       = toNodeId;
    dataForSend.operationType  = operationType;
    dataForSend.fromNodeId     = fromNodeId;
    dataForSend.value          = value;
    dataForSend.onOffValue     = onOffValue;
    if(floatValue != NULL)
    {
        CopyCharArray(floatValue, dataForSend.floatValue, 6);
    }
    else
    {
        unsigned char defaultValue[] = { '0', '0', '0', '0', '0', '0'};
        CopyCharArray(defaultValue, dataForSend.floatValue, 6);
    }
    
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


void setupRadio(uint8_t **readAddresses, int childsCount, uint8_t writeAddress[], size_t payloadSize, uint8_t channel, rf24_datarate_e dataRate)
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
    if(childsCount == 0)
    {
        for(int i = 1; i <= 5; i++)
        {
            radio.openReadingPipe(i, 0xffffffffff);
        }
    }
    else
    {
        for(int i = 1; i <= childsCount; i++)
        {
            radio.openReadingPipe(i, readAddresses[i - 1]);
        }    
    }
    

    radio.printDetails();
}

void activeNode(size_t payloadSize, unsigned long maxtime)
{
    //Operacion tipo 0 indica ninguna operacion por hacer
    syncDataForSend.operationType = 0;
    
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
             if(CompareCharArray(tempNodeIdForActivate, tempNodeIdDefaultValue, 13))
             {
                    CopyCharArray(syncDataForReceive.tempNodeId, tempNodeIdForActivate, 13);
             }
             //Con esto nos aseguramos que los mensajes que se reciban solo se procecen aquellos del nodo
             //que solicito (del quien se recibio mensaje primero)
             if(CompareCharArray(syncDataForReceive.tempNodeId, tempNodeIdForActivate, 13))
             {
                
                //Si el nodo esta solicitando una direccion
                if(syncDataForReceive.operationType == RF_REQUEST_ADDRESS)
                {
                    // Se obtiene una direccion disponible, esta direccion sera la direccion de escritura
                    //del nodo
                    Address writeAddress = GetAvailableAddress();

                    // Se establece la operacion de respuesta de direccion
                    syncDataForSend.operationType   = RF_RESPONSE_ADDRESS;
                    CopyCharArray(syncDataForReceive.tempNodeId,syncDataForSend.tempNodeId, 13);
                    //Se copia la direccion de escritura del nodo
                    CopyCharArray(writeAddress.address, syncDataForSend.address, 5);

                    // Cuando el valor de  value es igual a cero, significa que este mensaje no ha sido retransmitido
                    //por lo tanto este nodo esta cerca del nodo cero (servidor)
                    if(syncDataForReceive.fatherId ==  0)
                    {
                        syncDataForSend.fatherId        = nodesCount;
                        // Se guarda la direccion de lectura y se envia en la variable address2
                        CopyCharArray(root->rfNode.address, syncDataForSend.address2, 5);
                        nodeActivationPending = new Node(nodesCount, writeAddress.address);
                        root->AddChild(nodeActivationPending->rfNode.nodeId, nodeActivationPending->rfNode.address);

                    }
                    else // El nodo esta cerca de otro nodo, entonces se toma como padre del nodo
                    {
                        
                    }
                }
                // El nodo responde que ya tiene la direccion guardada, esta respuesta tambien puede llegar
                // a el servidor en modo operativo normal
                else if(syncDataForReceive.operationType == RF_RESPONSE_ADDRESS_RECEIVED)
                {

                }
             }
        }

        
        //Retraso de 100 milisegundos
        delay(50);
        
    }

}

void AddLineToFile(char * fileName, string newLine)
{
    std::ofstream file(fileName, std::ios_base::app | std::ios_base::out);
    file << newLine << "\n";
    file.close();
}


//Actualiza el archivo
bool updateFile(char * fileName, string oldLine, string newLine)
{
    string readLine;
    ifstream file (fileName);
    ofstream ofile ( "temp.txt" );
    while (!file.eof())
    {
        getline(file, readLine);
        if (readLine == oldLine)
        {
            ofile << newLine << endl;
        }
        else
            ofile << readLine << endl;
    }

    file.close();
    ofile.close();
    remove(fileName);
    return rename("temp.txt", fileName);
}

//
string ConvertAddressToCommasFormat(unsigned char * addressCharArray)
{
    string address ((char *)addressCharArray);
    string addressWithCommas = "";
    int length = (int)address.size();
    for(int i = 0; i < length; i++)
    {
        char characterWithComma[] = { address[i], ',', 0};
        string characterWithCommaString (characterWithComma);
        addressWithCommas.append(characterWithCommaString);
    }

    return addressWithCommas;
}


void disableAddress(char fileName[], unsigned char * addressCharArray)
{
    string address ((char *)addressCharArray);
    string addressWithCommas = "";
    int length = (int)address.size();
    for(int i = 0; i < length; i++)
    {
        char characterWithComma[] = { address[i], ',', 0};
        string characterWithCommaString (characterWithComma);
        addressWithCommas.append(characterWithCommaString);
    }

    string disabledString   = " 1";
    string enabledString    = " 0";
    string newLine = addressWithCommas + disabledString;
    string oldLine = addressWithCommas + enabledString;

    updateFile(fileName, oldLine, newLine);
}


void saveNode(char fileName[], unsigned char nodeId, unsigned char fatherId, unsigned char * addressCharArray)
{
    string nodeIdString = std::to_string(nodeId);
    string fatherIdString = std::to_string(fatherId);
    string addressStringWithCommasFormat = ConvertAddressToCommasFormat(addressCharArray);
    string newLine = fatherIdString + " " + nodeIdString + " " + addressStringWithCommasFormat;
    AddLineToFile(fileName, newLine);
    
}



void PerformOperation(dataStruct data)
{
    switch (mode)
    {
        case ACTIVATE_MODE :
                    switch (data.operationType)
                    {

                        case RF_SENSING:
                            
                            //Si existe un nodo que estaba en modo activacion, entonces cuando se active empezara a sensar, por lo tanto hay que
                            // preguntar si el nodo que envia el mensaje es el que se estaba activando, y si es asi avisar a el servidor a que termine
                            // el proceso de activacion
                            if(nodeActivationPending != NULL && nodeActivationPending->rfNode.nodeId == data.fromNodeId)
                            {
                                //Se guarda el nodo
                                saveNode(nodesFileName, nodeActivationPending->rfNode.nodeId, fatherIdOfTheNodeActivationPending, nodeActivationPending->rfNode.address);
                                
                                //Se deshabilita la direccion que ocupo el nodo
                                disableAddress(addressesFileName, nodeActivationPending->rfNode.address);

                                // Se reestablece el valor de tempNodeIdForActivate
                                CopyCharArray(tempNodeIdDefaultValue, tempNodeIdForActivate, 13);

                                // IMPORTANTE, EL MISMO SERVIDOR RF ESTA CAMBIANDO EL MODO EN QUE OPERA ASIGNANDO EL COMANDO
                                // ES IMPORTANTE AVISAR A EL SERVIDOR WEB, ESTO LO HACE PARA SALIR DEL ESTADO ACTIVATE_MODE Y NO GUARDAR DE NUEVO EL NODO
                                command = ENABLE_NORMAL_MODE;
                                //SendMessageToWebServer();
                            }

                            
                        break;

                        case RF_RESPONSE_ADDRESS_RECEIVED:
                        
                            //Automaticamente se pasa a modo normal
                            //mode = NORMAL_MODE;
                            //El id del nodo temporal se vuelve
                            //CopyCharArray(tempNodeIdDefaultValue, tempNodeIdForActivate, 13);

                            // Avisar a el servidor que el nodo esta activado y que se pasa a estado normal
                        break;

                        //Se recibe una respuesta de un nodo que ya esta en nodo broadcast, entonces, se debe de registrar
                        // el node que se esta en modo broadcast
                        case RF_RESPONSE_BROADCAST_MODE:
                        
                        break;
                        

                        default:
                            break;
                    }
                    break;
        
        
        case NORMAL_MODE:
        default:
                switch (data.operationType)
                {
                    case RF_SENSING:
                    //SendData to server
                    break;
                
                default:
                    break;
                }
                break;
    }
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
    unsigned char ** readAddresses = new unsigned char*[1];

    switch (command)
    {
        case ENABLE_BROADCAST_MODE:
            switch (mode)
            {
                case ACTIVATE_MODE:
                case NORMAL_MODE:
                    cout << "Activate node mode on\n";
                    readAddresses[0] = GetDefaultReadAddress().address;
                    setupRadio(readAddresses, 1, GetDefaultWriteAddress().address, sizeof(syncStruct), 120, RF24_250KBPS);
                    activeNode(sizeof(syncStruct), 5000);
                    cout << "Activate node mode off\n";
                    cout << "Activate normal mode on\n";
                    setupRadio(root->GetChildsAddresses(), root->GetChildsCount(), root->rfNode.address, sizeof(dataStruct), 120, RF24_250KBPS);
                    setDataForSend(0, 1, 150, '1', NULL, false);
                    normalOperationWithTimeout(50, 5000);
                    mode = ACTIVATE_MODE;
                    PerformOperation(dataForReceive);
                    cout << "Activate normal mode off\n";
                break;
        
            default:
                break;
            }
            
            break;
            
        case ENABLE_NORMAL_MODE:
            switch (mode)
            {
                case ACTIVATE_MODE:
                    setupRadio(root->GetChildsAddresses(), root->GetChildsCount(), root->rfNode.address, sizeof(dataStruct), 120, RF24_250KBPS);
                    mode = NORMAL_MODE;
                case NORMAL_MODE:
                    setDataForSend(root->rfNode.nodeId, NODEID_FOR_BROADCAST, RF_SENSING, 0, defaultFloatValue , false);
                    normalOperation(50);
                    PerformOperation(dataForReceive);
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
    addresses = GetAddreses(addressesFileName);
    //printAdresses();
    
    root = BuildTree(nodesFileName);
    //root->rfNode.PrintAddress(0);
     command = ENABLE_NORMAL_MODE;

     //command = ENABLE_BROADCAST_MODE;

     // Se configura el radio en modo normal (sensando) operativo
     setupRadio(root->GetChildsAddresses(), root->GetChildsCount(), root->rfNode.address, sizeof(dataStruct), 120, RF24_250KBPS);
    
    while(true)
    {
        // Direcciona el comando recibido
        AddressCommand(command);
    }
    
    return 0;
}