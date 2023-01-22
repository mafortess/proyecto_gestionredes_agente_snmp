// MiguelJose.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
#include <iostream>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <sys/types.h>
#include <winsock2.h>
#include <stdlib.h>
#include "Lista.h"


#define FICH_TRAZAS "obex8.log"  // Cambiar <grupo> por el identificador del grupo

//PUERTO DE ESCUCHA SERVIDOR
#define PUERTO 161

// SNMP
#define MAX_MENSAJE_SNMP	2048


// Funciones auxiliares
#define LOG(s)	fprintf(flog, "%s", s); fflush(flog);

#define LOGP(s)	fprintf(flog, "%s", s); printf("%s", s); fflush(flog);

#define LOGBYTE(s, len)	{ int i; \
						  fprintf(flog, "\t");          \
						  for (i=0; i<len; i++) \
						  {                      \
							fprintf(flog, "0x%2.2X ", (unsigned char) s[i]); \
							if (((i+1)%16 == 0) && (i != 0))                  \
								fprintf(flog, "\n\t");          \
						  }                                 \
						  LOG("\n"); \
						  fflush(flog); \
						}

#define LOGPBYTE(s, len)	{ int i; \
  							  fprintf(flog, "\t");          \
							  printf("\t");          \
							  for (i=0; i<len; i++) \
							  {                      \
								fprintf(flog, "0x%2.2X ", (unsigned char) s[i]); \
								printf("0x%2.2X ", (unsigned char) s[i]); \
								if (((i+1)%16 == 0) && (i != 0))  {                 \
								fprintf(flog, "\n\t");          \
								printf("\n\t"); }                \
							  }                                 \
							  LOGP("\n"); \
							  fflush(flog); \
							}

#define COMPRUEBA_OCTETO(cad, pos, c) \
    { if (cad[pos] != c) \
      { \
        LOGP("Error: Formato incorrecto. Esperado 0x%2.0X   Hallado 0x%2.0X", \
              c, cad[pos]);printf("\n");\
        getchar(); \
        return; \
      } \
      else \
        LOGP("OK: Formato correcto. Esperado 0x%2.0X   Hallado 0x%2.0X", \
              c, cad[pos]);printf("\n");\
    }

/* Posible definición de los tipos de datos básicos */
typedef union
{
	int val_int;   /* para tipo INTEGER */
	char *val_cad; /* para OCTET STRING, OBJECT IDENTIFIER, IpAddress */
} tvalor;

typedef struct valor  /* guardar los valores de las celdas de una tabla */
{
	tvalor val;              /* valor guardado */
	struct valor *sig_fila;  /* apunta al valor de la siguiente fila */
	struct valor *sig_col;   /* apunta al valor de la misma fila en la
							  siguiente columna */
} nvalor;

typedef struct nodo
{
	char* nombre; //AÑADIDO
	int tipo_obj;  /* escalar (0), nodo tabla (1), nodo fila (2),
					nodo columna (3) */
	int tipo_de_dato;  /* valores posibles, ej: INTEGER (0),
						OCTET STRING (1), IpAddress(2)... */
	int acceso;    /* valores posibles, ej: not-accessible (0),
					read-only (1), read-write (2) */
					/* La cláusula STATUS no se guarda porque sólo se almacenan los
					   nodos 'current'. */
	char *oid;     /* para que sea dinámico; se puede comparar como
					una cadena de texto */
	nvalor tipo_valor;
	int sig;  /* apunta al siguiente nodo de la lista */
	struct nodo *indice;  /* apunta al nodo columna índice de esta tabla;
						   sólo tiene sentido si es un nodo fila */
} nodo;

FILE *flog;
SOCKET sock;
sockaddr_in servidor, cliente;
char * OID_ = new char[100];

using namespace std;
using namespace NPLista;

//Método que carga la MIB en el agente
//La MIB se inserta en una lista dinámica implementada en (Lista.h)
void cargarMIB(ClaseLista<nodo> &MIB);
int leerPeticion(ClaseLista<nodo> &MIB, const char* buffer, const unsigned short tam, char* respuesta);
int bytesToInt(const char* buff, unsigned short tam);
unsigned short leerTLV(const char * buffer, unsigned short &T, nvalor &V);
unsigned short getTipo(const char* buffer, unsigned short &pos);
unsigned short getLongitud(const char* buffer, unsigned short &pos);
void getInt(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V);
void getOctetString(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V);
void getIpAddress(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V);
void getOID(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V);
unsigned short calcularOID(ClaseLista<nodo> &MIB, char* OID);
int procesarMensajeSNMP(ClaseLista<nodo> &MIB, char* respuesta, int num_pet, unsigned short operacion, unsigned short oid, nvalor &V, int error);
int crearMensajeRespuesta(ClaseLista<nodo> &MIB, char * respuesta, int num_pet, int error, nodo &aux);
void setInt(char* buffer, unsigned short &pos, int dato, unsigned short tam);
void setOctetString(char* buffer, unsigned short &pos, const char* cadena);

int main(int argc, char* argv[])
{
	/* Variables para inicialización de sockets */
	WORD wVersionRequested;
	WSADATA wsaData;
	int longitud_cliente = sizeof(cliente);
	int bytes_recv,tam_respuesta,bytes_send;
	ClaseLista<nodo> MIB;
	char buffer[MAX_MENSAJE_SNMP];
	char respuesta[MAX_MENSAJE_SNMP];


	/* Control del número de argumentos de entrada */
	if (argc != 2)  /* 1 ==> Sin argumentos; argv[0] es el nombre del programa */
	{
		printf("Numero de argumentos incorrecto.\n");
		exit(0);
	}

	// AÑADIDO
    // ofstream file1(FICH_TRAZAS);  (equivalente al fopen()) //AÑADIDO

	// Abrir el fichero de trazas (se sobreescribe cada vez)
	flog = fopen(FICH_TRAZAS, "w");
	if (flog == NULL)
	{
		printf("Error al crear el fichero de trazas\n");
		exit(1);
	}

	cargarMIB(MIB);
	LOGP("MIB cargada con exito\n");

	/* Inicialización de sockets en Windows */
	wVersionRequested = MAKEWORD(2, 2);
	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return -1;
	}
	LOGP("Sockets de Windows inicializados\n");

	//Inicializamos socket

	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == INVALID_SOCKET) {
		LOGP("Error al crear el socket\n");
		WSACleanup();
		return -1;
	}
	else {
		LOGP("Se ha creado el socket\n");
	}

	//Completamos la estructura sockaddr_in del servidor
	memset(&servidor, 0, sizeof(servidor));
	servidor.sin_port = htons(atoi(argv[1])); // htons(PORT);
	servidor.sin_family = AF_INET;
	servidor.sin_addr.s_addr = INADDR_ANY; //Atendemos a cualquier cliente

	//Realizamos bind en el socket
	if (bind(sock,(SOCKADDR *)&servidor,sizeof(servidor)) == SOCKET_ERROR) {
		LOGP("Error al realizar bind\n");
		closesocket(sock);
		WSACleanup();
		return -1;
	}
	else {
		LOGP("Bind realizado correctamente\n");
	}

	while (1) {

		//Reseteamos el buffer de recepción y el de respuesta
		memset(buffer, '\0', MAX_MENSAJE_SNMP);
		memcpy(buffer, "", MAX_MENSAJE_SNMP);
		memset(respuesta, '\0', MAX_MENSAJE_SNMP);
		memcpy(respuesta, "", MAX_MENSAJE_SNMP);
		printf("\n=============================================================================");
		printf("\nEsperando solicitud del gestor: \n");
		bytes_recv = recvfrom(sock,buffer,MAX_MENSAJE_SNMP,0, (SOCKADDR *)&cliente,&longitud_cliente);

		if (bytes_recv < 0) {
			LOGP("Error recvfrom\n");
		}

		buffer[bytes_recv] = '\0';
		printf("Solicitud del gestor:\n");
		printf("Recibido de direccion %s: %d Bytes\n", inet_ntoa(cliente.sin_addr),bytes_recv);
		fflush(stdout);
		LOGPBYTE(buffer, bytes_recv);

		tam_respuesta = leerPeticion(MIB, buffer, bytes_recv, respuesta);

		bytes_send = sendto(sock, respuesta, tam_respuesta, 0,(SOCKADDR *)&cliente,longitud_cliente);


		if (bytes_send < tam_respuesta) {
			printf("No se han enviado los bytes esperados\n");
		}
		else {
			printf("\nRespuesta enviada al gestor:\n");
			fflush(stdout);
			respuesta[bytes_send]='\0';
			LOGPBYTE(respuesta, bytes_send);		
		}

	}

	closesocket(sock);

	/* Cierre */
	if (flog != NULL)
		fclose(flog);
	exit(0);
}

//GET(1.3.6.1.3.53.8.2.0)
/*
{CABECERA SNMP:
    SEQUENCE:   30(T: sequence)    26(L: vble segun mensaje)    Contenido (V) 
    {    
        VERSION:    02 01 00 (tipo integer longitud 1 valor 0 (version 1))
        COMUNIDAD:  04(T: comunidad)   06(L)    70 (p)  75(u)   62(b)   6C(l)   69(i)   63(c)  
        TIPO PDU:  A0 (T: GET)  19(L: longitud PDU)      Contenido (V)
        {
            IDENTIF PETICION:   02 (T: integer)     01(L)       14(nº peticion = 20)
            ESTADO ERROR:       02 (T: integer)     01(L)       00 (no hay errores)
            INDEX ERROR:        02 (T: integer)     01(L)       00 (no hay errores)
            LISTA OBJ Y VALRS:  30(T: sequence)     0E(L:14)    Contenido (V)
            (varbindlist)
            {
                PAR instancia-VALOR:   30(T: sequence) 0C (L)      Contenido(V) 
                {
                    06 (T: OID)     08 (L)      2B 06 01 03 35 08 02 00 (V: OID)    
                    VALOR   05  (T: NULL)       00 (L)
                }
            }
            
        }
    } 
} 
*/

//############################################################

//Función que pasa a int el número de bytes indicado
int bytesToInt(const char* buff, unsigned short tam) {
	int val = 0;
	int j = 0;

	for (int i = tam - 1; i >= 0; --i) {
		val += ((unsigned char)buff[i] & 0xFF) << (8 * j);
		++j;
	}
	return val;
}

//Método para cargar la MIB en el agente en la estructura dinámica lista.h
//De momento, insertamos tipos escalares, no tabulares
void cargarMIB(ClaseLista <nodo> &MIB) {
	bool ok;
	nodo instancia[19];

	//Instancia de nombre de dispositivo
	instancia[0].nombre=(char *)"nombreDispositivo";
	instancia[0].tipo_obj = 0;
	instancia[0].tipo_de_dato = 1;
	instancia[0].acceso = 1;
	instancia[0].oid = (char *)"1.3.6.1.3.53.8.1.0";
	instancia[0].tipo_valor.val.val_cad = (char *)"PC01";
	//usamos la lista.h para realizar esto, por eso se pone a NULL
	instancia[0].sig = 2;
	instancia[0].indice = NULL;
	MIB.insertar(1, instancia[0], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	//Instancia de persona de contacto
	instancia[1].nombre=(char *)"personaContacto";
	instancia[1].tipo_obj = 0;
	instancia[1].tipo_de_dato = 1;
	instancia[1].acceso = 2;
	instancia[1].oid = (char *)"1.3.6.1.3.53.8.2.0";
	instancia[1].tipo_valor.val.val_cad = (char *)"Pepe";
	instancia[1].sig = 3;
	instancia[1].indice = NULL;
	MIB.insertar(2, instancia[1], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	//Instancia de contador de entradas
	instancia[2].nombre=(char *)"numPentrada";
	instancia[2].tipo_obj = 0;
	instancia[2].tipo_de_dato = 0;
	instancia[2].acceso = 2;
	instancia[2].oid = (char *)"1.3.6.1.3.53.8.3.0";
	instancia[2].tipo_valor.val.val_int = 20;
	instancia[2].sig = 4;
	instancia[2].indice = NULL;
	MIB.insertar(3, instancia[2], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	//Instancia de contador de salidas
	instancia[3].nombre=(char *)"numPsalida";
	instancia[3].tipo_obj = 0;
	instancia[3].tipo_de_dato = 0;
	instancia[3].acceso = 2;
	instancia[3].oid = (char *)"1.3.6.1.3.53.8.4.0";
	instancia[3].tipo_valor.val.val_int = 14;
	instancia[3].sig = 5;
	instancia[3].indice = NULL;
	MIB.insertar(4, instancia[3], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	//Instancia de direccion IP del dispositivo
	instancia[4].tipo_valor.val.val_cad = (char *)"ipDispositivo";
	instancia[4].tipo_obj = 0;
	instancia[4].tipo_de_dato = 2;
	instancia[4].acceso = 2;
	instancia[4].oid = (char *)"1.3.6.1.3.53.8.5.0";
	instancia[4].tipo_valor.val.val_cad = (char *)"192.168.1.10";
	instancia[4].sig = 6;
	instancia[4].indice = NULL;
	MIB.insertar(5, instancia[4], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}
	
	 // TABLA DE HISTORICOS
 	instancia[5].tipo_valor.val.val_cad = (char *)"dia1";
    instancia[5].tipo_obj = 0;
    instancia[5].tipo_de_dato = 0;
    instancia[5].acceso = 2;
    instancia[5].oid = (char *)"1.3.6.1.3.53.8.6.1.1.1";
    instancia[5].tipo_valor.val.val_int = 1;
    instancia[5].sig = 7;
    instancia[5].indice = NULL;
	MIB.insertar(6, instancia[5], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[6].tipo_valor.val.val_cad = (char *)"dia2";
    instancia[6].tipo_obj = 0;
    instancia[6].tipo_de_dato = 0;
    instancia[6].acceso = 2;
    instancia[6].oid = (char *)"1.3.6.1.3.53.8.6.1.1.2";
    instancia[6].tipo_valor.val.val_int = 2;
    instancia[6].sig = 8;
    instancia[6].indice = NULL;
	MIB.insertar(7, instancia[6], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[7].tipo_valor.val.val_cad = (char *)"numEntrada1";
    instancia[7].tipo_obj = 0;
    instancia[7].tipo_de_dato = 0;
    instancia[7].acceso = 2;
    instancia[7].oid = (char *)"1.3.6.1.3.53.8.6.1.2.1";
    instancia[7].tipo_valor.val.val_int = 53;
    instancia[7].sig = 9;
    instancia[7].indice = NULL;
	MIB.insertar(8, instancia[7], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[8].tipo_valor.val.val_cad = (char *)"numEntrada2";
    instancia[8].tipo_obj = 0;
    instancia[8].tipo_de_dato = 0;
    instancia[8].acceso = 2;
    instancia[8].oid = (char *)"1.3.6.1.3.53.8.6.1.2.2";
    instancia[8].tipo_valor.val.val_int = 62;
    instancia[8].sig = 10;
    instancia[8].indice = NULL;
	MIB.insertar(9, instancia[8], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[9].tipo_valor.val.val_cad = (char *)"numSalida1";
    instancia[9].tipo_obj = 0;
    instancia[9].tipo_de_dato = 0;
    instancia[9].acceso = 2;
    instancia[9].oid = (char *)"1.3.6.1.3.53.8.6.1.3.1";
    instancia[9].tipo_valor.val.val_int = 32;
    instancia[9].sig = 11;
    instancia[9].indice = NULL;
	MIB.insertar(10, instancia[9], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[10].tipo_valor.val.val_cad = (char *)"numSalida2";
    instancia[10].tipo_obj = 0;
    instancia[10].tipo_de_dato = 0;
    instancia[10].acceso = 2;
    instancia[10].oid = (char *)"1.3.6.1.3.53.8.6.1.3.2";
    instancia[10].tipo_valor.val.val_int = 16;
    instancia[10].sig = 12; //&instancia[11];
    instancia[10].indice = NULL;
	MIB.insertar(11, instancia[10], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

    // TABLA DE DISPOSITIVOS
    instancia[11].tipo_valor.val.val_cad = (char *)"ipDispositivo1";
	instancia[11].tipo_obj = 0;
    instancia[11].tipo_de_dato = 1;
    instancia[11].acceso = 2;
    instancia[11].oid = (char *)"1.3.6.1.3.53.8.7.1.1.8.8.8.8";
    instancia[11].tipo_valor.val.val_cad = (char *)"8.8.8.8";
    instancia[11].sig = 13;
    instancia[11].indice = NULL;
	MIB.insertar(12, instancia[11], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[12].tipo_valor.val.val_cad = (char *)"ipDispositivo2";
    instancia[12].tipo_de_dato = 1;
    instancia[12].acceso = 2;
    instancia[12].oid = (char *)"1.3.6.1.3.53.8.7.1.1.9.9.9.9";
    instancia[12].tipo_valor.val.val_cad = (char *)"9.9.9.9";
    instancia[12].sig = 14; //&instancia[13];
    instancia[12].indice = NULL;
	MIB.insertar(13, instancia[12], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[13].tipo_valor.val.val_cad = (char *)"modeloDispositivo1";
    instancia[13].tipo_obj = 0;
    instancia[13].tipo_de_dato = 0;
    instancia[13].acceso = 2;
    instancia[13].oid = (char *)"1.3.6.1.3.53.8.7.1.2.8.8.8.8";
    instancia[13].tipo_valor.val.val_int = 2;
    instancia[13].sig = 15;
    instancia[13].indice = NULL;
	MIB.insertar(14, instancia[13], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[14].tipo_valor.val.val_cad = (char *)"modeloDispositivo2";
    instancia[14].tipo_obj = 0;
    instancia[14].tipo_de_dato = 0;
    instancia[14].acceso = 2;
    instancia[14].oid = (char *)"1.3.6.1.3.53.8.7.1.2.9.9.9.9";
    instancia[14].tipo_valor.val.val_int = 2;
    instancia[14].sig = 16; //&instancia[15];
    instancia[14].indice = NULL;
	MIB.insertar(15, instancia[14], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[15].tipo_valor.val.val_cad = (char *)"tipoTarjeta1";
    instancia[15].tipo_obj = 0;
    instancia[15].tipo_de_dato = 1;
    instancia[15].acceso = 2;
    instancia[15].oid = (char *)"1.3.6.1.3.53.8.7.1.3.8.8.8.8";
    instancia[15].tipo_valor.val.val_cad = (char *)"PCI-express-1";
    instancia[15].sig = 17;
    instancia[15].indice = NULL;
	MIB.insertar(16, instancia[15], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[16].tipo_valor.val.val_cad = (char *)"tipoTarjeta2";
    instancia[16].tipo_obj = 0;
    instancia[16].tipo_de_dato = 1;
    instancia[16].acceso = 2;
    instancia[16].oid = (char *)"1.3.6.1.3.53.8.7.1.3.9.9.9.9";
    instancia[16].tipo_valor.val.val_cad = (char *)"PCI-express-2";
    instancia[16].sig = 18; //&instancia[16];
    instancia[16].indice = NULL;
	MIB.insertar(17, instancia[16], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}

	instancia[17].tipo_valor.val.val_cad = (char *)"fechaInstalacion1";
    instancia[17].tipo_obj = 0;
    instancia[17].tipo_de_dato = 1;
    instancia[17].acceso = 2;
    instancia[17].oid = (char *)"1.3.6.1.3.53.8.7.1.4.8.8.8.8";
    instancia[17].tipo_valor.val.val_cad = (char *)"22/07/10";
    instancia[17].sig = 19; // &instancia[4];
    instancia[17].indice = NULL;
	MIB.insertar(18, instancia[17], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}
	
	instancia[18].tipo_valor.val.val_cad = (char *)"fechaInstalacion2";
    instancia[18].tipo_obj = 0;
    instancia[18].tipo_de_dato = 1;
    instancia[18].acceso = 2;
    instancia[18].oid = (char *)"1.3.6.1.3.53.8.7.1.4.9.9.9.9";
    instancia[18].tipo_valor.val.val_cad = (char *)"25/07/10";
    instancia[18].sig = 0; 
    instancia[18].indice = NULL;	
	MIB.insertar(19, instancia[18], ok);

	if (!ok) {
		printf("Error al insertar MIB\n");
	}
}

//Funcion que nos devuelve el tipo 
unsigned short getTipo(const char* buffer, unsigned short &pos) {
	int T = bytesToInt(buffer, 1);
	pos++;
	return T;
}

//Funcion que calcula la longitud del tipo
unsigned short getLongitud(const char* buffer, unsigned short &pos) {
	unsigned short n = 1;
	unsigned short L = (unsigned short)buffer[pos];

	if (L > 127) {
		n = L - 128;
		L = bytesToInt(&buffer[1], n);
		pos += n + 1;
	}
	else {
		pos++;
	}
	return L;
}

//Método que nos devuelve int
void getInt(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V) {
	int aux = bytesToInt(&buffer[pos], tam);
	pos += tam;
	V.val.val_int = aux;
}

//Método que nos devuelve el octet String
void getOctetString(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V) {
	unsigned short i;
	char *aux = new char[tam];
	for (i = 0; i < tam; i++) {
		aux[i] = buffer[pos + i];
	}
	aux[i] = '\0';
	V.val.val_cad = aux;
	pos += tam;
}

void getIpAddress(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V) {
	char *aux = new char[32];
	int dig1, dig2, dig3, dig4;
	//Obtenemos cada número de direccion ip en entero
	dig1 = bytesToInt(&buffer[pos], 1);
	dig2 = bytesToInt(&buffer[pos+1], 1);
	dig3 = bytesToInt(&buffer[pos + 2], 1);
	dig4 = bytesToInt(&buffer[pos + 3], 1);
	sprintf(aux, "%d.%d.%d.%d", dig1, dig2, dig3, dig4);
	V.val.val_cad = aux;
	pos += 4;
}

void getOID(const char* buffer, unsigned short &pos, const unsigned short tam, nvalor &V) {
	//Obtenemos y calculamos el primer par que forman el OID
	int x = buffer[pos] / 40;
	int y = buffer[pos] - (x * 40);
	char *aux = new char[100];
	char octec[10];
	unsigned short i;

	//Concatenamos en aux el primer y segundo subidentificador
	sprintf(aux, "%d.%d", x, y);
	for (i = 1; i < tam; i++) {
		//concatenamos el octect el resto de subidentificadores
		sprintf(octec, ".%d", buffer[pos + i]);
		//concatenamos en aux el valor octect, por tanto, aux ya es el OID entero
		strcat(aux, octec);
	}

	V.val.val_cad = aux;
	pos = pos + tam;
}

//devuelve indice de el oid en la estructura de la lista o 0 si no pertenece
unsigned short calcularOID(ClaseLista<nodo> &MIB, char* OID) {
	int i = 0, tam = MIB.obtenerLongitud();
	int tam2 = strlen(OID);
	unsigned short oid;
	bool ok;
	char * dato = new char[10];

	//comprobar si ese oid pertenece a la MIB sino se mandará un 0
	nodo elemento;
	do {
		i++;
		if (i <= tam) {
			MIB.consultar(i, elemento, ok);
			if ((strcmp(OID, elemento.oid) == 0)) {
				break;
			}
		}
	} while (i <= tam);

	//El oid no pertenece a la mib
	if (i > tam) {
		oid = 0;
	}
	else { //posicion en la lista del oid i
		oid = i;
	}

	return oid;
}

void setInt(char* buffer, unsigned short &pos, int dato, unsigned short tam) {
	buffer[pos++] = (char)2; //Integer (0x02) //Tipo
	buffer[pos++] = (char)tam; //Longitud
	//Se contempla 4 tipos de tamaño, 1 octeto, 2, 3 y 4 
	if (tam == 1) {
		buffer[pos++] = (char)dato;
	}
	else if (tam == 2) {
		buffer[pos++] = (dato >> 8) & 0xFF;
		buffer[pos++] = dato & 0xFF;
	}
	else if (tam == 3) {
		buffer[pos++] = (dato >> 16) & 0xFF;
		buffer[pos++] = (dato >> 8) & 0xFF;
		buffer[pos++] = dato & 0xFF;
	}
	else if (tam == 4) {
		buffer[pos++] = (dato >> 24) & 0xFF;
		buffer[pos++] = (dato >> 16) & 0xFF;
		buffer[pos++] = (dato >> 8) & 0xFF;
		buffer[pos++] = dato & 0xFF;
	}
}

void setOctetString(char* buffer, unsigned short &pos, const char* cadena) {
	int tam = strlen(cadena);
	buffer[pos++] = (char)4; //OctetString (0x04) //Tipo
	buffer[pos++] = (char)tam; //Longitud
	for (int i = 0; i < tam; i++) {
		buffer[pos + i] = cadena[i]; //Valor
	}
	pos += tam;
}

void setIpAddress(char* buffer, unsigned short &pos, const char* cadena) {
	int tam = strlen(cadena);
	buffer[pos++] = (char)64;
	buffer[pos++] = (char)4;
	char octec[3];
	int j = 0, i = 0;

	for (int k = 0; k < 4; k++) {
		octec[0] = octec[1] = octec[2] = '\0';
		for (i = 0; cadena[j] != '.'&&j < tam; i++) {
			octec[i] = cadena[j];
			j++;
		}
		j++;
		buffer[pos++] = (char)atoi(octec);
	}
}

void setOID(char* buffer, unsigned short &pos, const char* cadena) {
	int tam = strlen(cadena);
	char octec[4];
	int L = 0, j = 0, i = 0;
	for (i = 0; i < tam; i++) {
		if (cadena[i] == '.') {
			L++;
		}
	}
	buffer[pos++] = (char)6;
	buffer[pos++] = (char)L;


	for (int k = 0; k < L; k++) {
		octec[0] = octec[1] = octec[2] = octec[3] = '\0';
		for (i = 0; cadena[j] != '.'&&j < tam; i++) {
			octec[i] = cadena[j];
			j++;
		}
		j++;
		if (k == 0) {
			int y, x = atoi(octec);
			octec[0] = octec[1] = octec[2] = octec[3] = '\0';
			for (i = 0; cadena[j] != '.'&&j < tam; i++) {
				octec[i] = cadena[j];
				j++;
			}
			j++;
			y = atoi(octec);
			buffer[pos++] = (char)((40 * x) + y);
		}
		else {
			buffer[pos++] = (char)atoi(octec);
		}
	}
}

//Funcion que lee el tipo, longitud y valor
unsigned short leerTLV(const char * buffer, unsigned short &T, nvalor &V) {
	unsigned short pos = 0, ind = 0, L = 0;

	T = getTipo(buffer, pos); //Obtengo Tipo de dato
	L = getLongitud(buffer, pos);//Obtengo longitud

	//El valor de tipo es mirado en base decimal, no en hexadecimal
	switch (T) {
	case 2: //Integer (0x02)
		getInt(buffer, pos, L, V); break;

	case 4: //Octet String (0x04)
		getOctetString(buffer, pos, L, V); break;

	case 6: //OID (0x06) 
		getOID(buffer, pos, L, V); break;

	case 48: //Secuence (0x30)
		ind = leerTLV(&buffer[pos], T, V); break;

	case 64: //IpAddress(0x64)
		printf("El tipo es IpAddress\n");
		getIpAddress(buffer, pos, L, V); break;

	case 160: //Get (0xA0)
		printf("|\t\tOperacion solicitada: GET\t\t|\n"); break;

	case 161: //GetNext (0xA1)
		printf("|\t\tOperacion solicitada: GETNEXT\t\t|\n"); break;

	case 163: //Set (0xA3)
		printf("|\t\tOperacion solicitada: SET\t\t|\n"); break;
	}

	//Devuelvo la posición por la que voy en el buffer para futuras llamadas
	ind = ind + pos;
	return ind;
}

//Funcion que lee la petición recibida del gestor para un sólo OID
int leerPeticion(ClaseLista<nodo> &MIB, const char* buffer, const unsigned short tam, char* respuesta){
	unsigned short operacion, oid, T=0, pos=0, ind=0;
	int num_pet, error=0;
	nvalor V;

	//Obtengo version SNMP y si no es la 1, marco el error para procesarlo en la respuesta 
	ind = ind + leerTLV(&buffer[ind],T,V);
	printf("---------------------------------------------------------\n");
	printf("|\t\tVersion SNMP: %d\t\t\t\t|\n", V.val.val_int+1);
	if (V.val.val_int > 0) {
		printf("Version SNMP no soportada. Se admite SNMP1\n");
		error = 5; //genError
	}
	else {
		//Obtengo nombre comunidad
		ind = ind + leerTLV(&buffer[ind], T, V);
		printf("|\t\tComunidad:%s\t\t\t|\n", V.val.val_cad);
		if (strcmp(V.val.val_cad, "public") != 0) {
			printf("El nombre de comunidad no es valido\n");
				error = 5;
		}
		
		//Obtengo tipo de PDU, el tipo PDU es imprimido en leerTLV
		ind = ind + leerTLV(&buffer[ind], T, V);
		operacion = T;

		//Obtengo el número de petición
		ind = ind + leerTLV(&buffer[ind], T, V);
		num_pet = V.val.val_int;
		printf("|\t\tNumero de peticion: %d\t\t\t|\n", num_pet);
		printf("---------------------------------------------------------\n");
		//Obtengo parte de error (ambos van a cero)
		ind = ind + leerTLV(&buffer[ind], T, V); //Obtengo tipo de error
		ind = ind + leerTLV(&buffer[ind], T, V); //Obtengo indice de error

		int i;
		//Secuencia de lista de instancias
		//do{
			//Obtengo OID
		ind = ind + leerTLV(&buffer[ind], T, V);
		strcpy(OID_, V.val.val_cad);
		//printf("OID solicitado: %s\n", V.val.val_cad);
		printf("|\tOID solicitado: %s\t|\n", OID_);
		oid = calcularOID(MIB, V.val.val_cad);
		//Obtengo el valor para ese OID
		ind = ind + leerTLV(&buffer[ind], T, V);

		switch (T) { //Dependiendo del tipo, sera una cadena, un entero, un ipaddress o el valor a NULL (Peticion GET O NEXTGET)
		case 2: //Int (0x02)
			printf("|\t\tValor: %d\t\t\t\t|\n", V.val.val_int); break;

		case 4: //Octet String (0x04)
			printf("|\t\tValor: %s\t\t\t\t|\n", V.val.val_cad); break;

		case 64: //IpAddress (0x64)
			printf("|\t\tValor: %s\t\t\t\t|\n", V.val.val_cad); break;

		case 5: //NULL
			printf("|\t\tValor: NULL\t\t\t\t|\n"); break;
		}
		//} while (buffer[ind] != NULL);	
	}
	printf("---------------------------------------------------------\n");
	if (error == 5) { //Si falla la version o el nombre de comunidad, no se envia nada
		return 0;
	}

	return procesarMensajeSNMP(MIB,respuesta,num_pet,operacion,oid,V,error);
}

int procesarMensajeSNMP(ClaseLista<nodo> &MIB, char* respuesta, int num_pet, unsigned short operacion, unsigned short oid, nvalor &V, int error) {
	nodo aux;
	bool ok;

	printf("\n\nProcesando mensaje SNMP recibido \n\n");
	/*noError(0),tooBig(1),noSuchName(2),badValue(3),readOnly(4) genErr(5)*/
	if (error != 5) {
		MIB.consultar(oid, aux, ok); //Obtenemos el nodo con el oid de la instancia pedida
		if (oid == 0) {
			printf("OID no pertenece a la MIB\n");
			error = 2;
		}
		else {
			if (aux.acceso == 0) {
				printf("El atributo \"MAX-ACCESS\" esta a: \"not-accessible\"\n");
				error = 4;
			}
			else if ((aux.acceso == 1) && (operacion == 163)) { //Si es sólo read-only y la operacion es de tipo SET
				printf("El atributo \"MAX-ACCESS\" esta a: \"read-only\"\n");
				error = 4;
			}else {
				//unsigned short OID_=calcularOID();

				switch (operacion) { //Si no hay problemas por el atributo MAX-ACCESS comprobamos el tipo de operacion
									//Para realizar una u otra acción
				case 160: //GET (0xA0)
					if (aux.tipo_de_dato == 0) { //Si el tipo de dato es INTEGER
						printf("|\tOID solicitado: %s\t|\n", OID_);
						printf("Valor: %d\n", aux.tipo_valor.val.val_int);
					}else 
					{ //Tipo de dato es Octet String, IpAddress
						printf("Valor: %s\n", aux.tipo_valor.val.val_cad);
					}
					break;
				case 161: //GETNEXT (0xA1)
					if (aux.sig == 0) { //Es la ultima instancia de la MIB
						printf("Error GETNEXT!! Fin de la MIB, no hay mas instancias!!\n\n");
						error = 5;
					}
					else {
						MIB.consultar(aux.sig, aux, ok); //aux es la nueva instancia con su OID
						if (aux.tipo_de_dato == 0) { //Si el tipo de dato es INTEGER
							printf("Valor de la instancia siguiente a enviar: %d\n", aux.tipo_valor.val.val_int);
						}
						else
						{ //Tipo de dato es Octet String, IpAddress
							printf("Valor de la instancia siguiente a enviar : %s\n", aux.tipo_valor.val.val_cad);
						}
					}
					break;

				case 163: //SET (0xA3)
					if (aux.tipo_de_dato == 0) { //INTEGER
						aux.tipo_valor.val.val_int = V.val.val_int;
						MIB.eliminar(oid, ok); //Elimino el nodo anterior
						MIB.insertar(oid, aux, ok); //Inserto el nuevo nodo
						printf("Nuevo valor de la instancia: %d\n", V.val.val_int);
					}
					else if(aux.tipo_de_dato == 1) { //OCTET STRING
						aux.tipo_valor.val.val_cad = V.val.val_cad;
						MIB.eliminar(oid, ok); //Elimino el nodo anterior
						MIB.insertar(oid, aux, ok); //Inserto el nuevo nodo
						printf("Nuevo valor de la instancia: %s\n", V.val.val_cad);
					}
					break;
				}
			}
		}
	}

	return crearMensajeRespuesta(MIB,respuesta, num_pet, error, aux);
}

int crearMensajeRespuesta(ClaseLista<nodo> &MIB,char * respuesta, int num_pet, int error, nodo &aux) {
	unsigned short pos = 0, n = 4;
	char temp[300];
	int tam1 = 0, tam2 = 0;
	
	if ((error != 2) && (error!=5)) {
		setOID(temp, n, aux.oid);
	}
	else {
		setOID(temp, n, OID_);
		temp[n++] = (unsigned char)5;
		temp[n++] = (unsigned char)0;
	}

	if (error != 2) {

		if (aux.tipo_de_dato == 0) {
			setInt(temp, n, aux.tipo_valor.val.val_int, 4);

		}
		else if (aux.tipo_de_dato == 1) {
			setOctetString(temp, n, aux.tipo_valor.val.val_cad);
		}
		else if (aux.tipo_de_dato == 2) {
			setIpAddress(temp, n, aux.tipo_valor.val.val_cad);
		}
		else if (aux.tipo_de_dato == 3) {
			setOID(temp, n, aux.tipo_valor.val.val_cad);
		}
	}
	temp[0] = (unsigned char)48;
	temp[1] = (unsigned char)n - 2;
	temp[2] = (unsigned char)48;
	temp[3] = (unsigned char)n - 4;
	tam1 = n;
	char temp2[300];
	n = 2;

	setInt(temp2, n, num_pet, 1);
	setInt(temp2, n, error, 1);
	if(error==0){
		setInt(temp2, n, 0, 1);
	}
	else {
		if (error != 2){
			setInt(temp2, n, calcularOID(MIB, aux.oid), 1);
		}
		else {
			setInt(temp2, n, 1, 1);
		}
	}
	temp2[0] = (unsigned char)162;
	tam2 = (tam1 + n - 2);
	temp2[1] = (unsigned char)tam2;

	//Secuencia
	respuesta[0] = (unsigned char)48; //0x30
	respuesta[1] = (unsigned char)(13+tam2);
	//Version
	respuesta[2] = (unsigned char)2; //0x02 INTEGER
	respuesta[3] = (unsigned char)1; //Longitud 1
	respuesta[4] = (unsigned char)0; //Valor 0: Versión SNMP 0
	//Comunidad
	respuesta[5] = (unsigned char)4; //0x04 Octet String
	respuesta[6] = (unsigned char)6; //Longitud 6
	respuesta[7] = (unsigned char)112; //P
	respuesta[8] = (unsigned char)117; //U
	respuesta[9] = (unsigned char)98;  //B
	respuesta[10] = (unsigned char)108;//L
	respuesta[11] = (unsigned char)105;//I
	respuesta[12] = (unsigned char)99; //C

	/*Response
	respuesta[13] = (unsigned char)162;
	respuesta[14] = tam2;
	*/
	int i = 0, k = 0;
	for (i = 13; i < n + 13; i++) {
		respuesta[i] = temp2[k++];
	}
	k = 0;
	for (; i < n + 13 + tam1; i++) {
		respuesta[i] = temp[k++];
	}

	if (error != 5) {
		return (n + 13 + tam1);
	}
	else {
		memset(&respuesta, 0,MAX_MENSAJE_SNMP);
		return 0;
	}
}

