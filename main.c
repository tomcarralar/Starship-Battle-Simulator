/*  Juan González Arranz 
    Tomás Carretero Alarcón 
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

struct lista{
    int disparosRecibidos;
    int disparosFallados;
    int botiquines;
    int idNave;
    struct lista* siguiente;
};

struct argDisp{
    char* rutaEntrada;
    char* rutaSalida;
    char* buffer;
    int tamanoBuffer;
};

struct argNave {
    char* buffer;
    struct lista* lista;
    int id;
    int tamanoBuffer;
}; 

struct argJuez {
    struct lista* lista; 
    char* rutaSalida; 
    int numNaves; 
};


sem_t bufferHayEspacio;
sem_t bufferHayDato;
sem_t mutexIndiceBuffer; //Solo se usa para las naves.
int indiceLecturaBuffer;

sem_t disparadorFinaliza;
sem_t naveFinaliza;
sem_t mutexLista;


void *disparador(void * arg){
    /* Lee el fichero de entrada, coloca los tokens en el buffer y al final escribe estadísticas en el
       fichero de salida.
    */
    struct argDisp* argumentos = (struct argDisp*) arg;
    
    FILE* entrada = fopen(argumentos->rutaEntrada, "r");
    FILE* salida = fopen(argumentos->rutaSalida, "w");
    char* buffer = argumentos->buffer;
    int tamanoBuffer = argumentos->tamanoBuffer;

    int token;
    int tokensValidos = 0;
    int tokensInvalidos = 0;
    int indice = 0;

    while ((token = getc(entrada)) != EOF){
        if (token == 'b'){ //Caso especial: botiquín
            token = getc(entrada);
            if (token == '1' || token == '2' || token == '3'){
                sem_wait(&bufferHayEspacio);
                buffer[indice] = (char) token;
                tokensValidos++;
                sem_post(&bufferHayDato);
                indice = (indice + 1) % tamanoBuffer; 
            } else {
                tokensInvalidos++;
            }
        } else if (token == ' ' || token == '*'){
            sem_wait(&bufferHayEspacio);
            buffer[indice] = (char) token;
            tokensValidos++;
            sem_post(&bufferHayDato);
            indice = (indice + 1) % tamanoBuffer; 
        } else {
            tokensInvalidos++;
        }
    }
    sem_wait(&bufferHayEspacio);
    buffer[indice] = 'f';
    sem_post(&bufferHayDato);
    fprintf(salida, "El disparador ha procesado: %d tokens válidos y %d tokens inválidos, total: %d\n", tokensValidos, tokensInvalidos, tokensValidos+tokensInvalidos);
    fclose(entrada);
    fclose(salida);
    sem_post(&disparadorFinaliza);
    pthread_exit(0);
}


void *nave(void * arg){
    /* Lee el buffer y va registrando sus movimientos.
       Al final debe guardar en una lista enlazada los datos obtenidos
    */
    struct argNave* argumentos = (struct argNave*) arg;
    struct lista* lista = argumentos->lista;
    char* buffer = argumentos->buffer;
    int tamanoBuffer = argumentos->tamanoBuffer;

    int disparosFallados = 0;
    int disparosRecibidos = 0;
    int botiquines = 0;
    char token;

    while(1){
        sem_wait(&bufferHayDato);
        sem_wait(&mutexIndiceBuffer);
        token = buffer[indiceLecturaBuffer];

        if (token == ' '){
            //INTENTO FALLIDO
            disparosFallados++;
            indiceLecturaBuffer = (indiceLecturaBuffer + 1) % tamanoBuffer;
            sem_post(&mutexIndiceBuffer);
            sem_post(&bufferHayEspacio);
        } else if (token == '*'){
            //IMPACTO
            disparosRecibidos++;
            indiceLecturaBuffer = (indiceLecturaBuffer + 1) % tamanoBuffer;
            sem_post(&mutexIndiceBuffer);
            sem_post(&bufferHayEspacio);
        } else if (token == 'f'){
            //FINAL
            sem_post(&mutexIndiceBuffer);
            sem_post(&bufferHayDato);
            break;
        } else {
            //BOTIQUIN
            botiquines++;
            if (token == '1'){
                //Borramos
                indiceLecturaBuffer = (indiceLecturaBuffer + 1) % tamanoBuffer;
                sem_post(&mutexIndiceBuffer);
                sem_post(&bufferHayEspacio);
            } else {
                //Actualizamos la cantidad de botiquines de ese token
                buffer[indiceLecturaBuffer] = token - 1;
                sem_post(&mutexIndiceBuffer);
                sem_post(&bufferHayDato);
            }
        }
    }
    //Escribimos
    sem_wait(&mutexLista);
    while (lista->siguiente != NULL){
        lista = lista->siguiente;
    }
    lista->botiquines = botiquines;
    lista->disparosRecibidos = disparosRecibidos;
    lista->disparosFallados = disparosFallados;
    lista->idNave = argumentos->id;

    if ((lista->siguiente = (struct lista*)malloc(sizeof(struct lista))) == NULL){
        fprintf(stderr, "ERROR RESERVANDO MEMORIA.");
        exit(-1);
    }
    lista->siguiente->siguiente = NULL;
    sem_post(&mutexLista);
    sem_post(&naveFinaliza);
    pthread_exit(0);
}


void *juez(void * arg){
    /* Recibe los datos de las naves que terminan y los escribe en un fichero.
       Termina escribiendo los datos de la nave ganadora y (si la hay) subcampeona 
       y luego estadísticas totales.
    */
    //Variables nave ganadora
    int disparosRecibidosGanadora = 0;
    int disparosFalladosGanadora = 0;
    int botiquinesGanadora = 0;
    int naveGanadora = -1;
    
    //Variables nave subcampeona
    int disparosRecibidosSubcampeona = 0;
    int disparosFalladosSubcampeona = 0;
    int botiquinesSubcampeona = 0;
    int naveSubcampeona = -1;

    //Variables resumen
    int disparosRecibidosTotales = 0; 
    int disparosFalladosTotales = 0; 
    int botiquinesEmitidosTotales = 0; 

    struct argJuez* argumentos = (struct argJuez*) arg;
    struct lista* datosNaves = argumentos->lista; 
    int numNavesJuzgadas = 0;

    sem_wait(&disparadorFinaliza);
    FILE* salida = fopen(argumentos->rutaSalida, "a");

    while(numNavesJuzgadas < argumentos->numNaves){
        sem_wait(&naveFinaliza);
        fprintf(salida, "Nave: %d\n", datosNaves->idNave);
        fprintf(salida, "\tDisparos recibidos: %d\n", datosNaves->disparosRecibidos);
        disparosRecibidosTotales += datosNaves->disparosRecibidos;  
        fprintf(salida, "\tDisparos fallados: %d\n", datosNaves->disparosFallados); 
        disparosFalladosTotales += datosNaves->disparosFallados;
        fprintf(salida, "\tBotiquines obtenidos: %d\n", datosNaves->botiquines);
        botiquinesEmitidosTotales += datosNaves->botiquines;
        
        int puntuacion =  datosNaves->disparosRecibidos - datosNaves->botiquines; 
        fprintf(salida, "\tPuntuación: %d\n\n", puntuacion); 

        //Actualizamos el ganador / subcampeon si se ha llegado a hacer algo
        if (!(datosNaves->disparosRecibidos == 0 && datosNaves->disparosFallados == 0 && datosNaves->botiquines == 0)){
            if (puntuacion < disparosRecibidosGanadora - botiquinesGanadora || naveGanadora == -1){
                //Pasamos el ganador al subcampeon
                disparosRecibidosSubcampeona = disparosRecibidosGanadora;
                disparosFalladosSubcampeona = disparosFalladosGanadora;
                botiquinesSubcampeona = botiquinesGanadora;
                naveSubcampeona = naveGanadora;

                //Nuevo ganador
                disparosRecibidosGanadora = datosNaves->disparosRecibidos;
                disparosFalladosGanadora = datosNaves->disparosFallados;
                botiquinesGanadora = datosNaves->botiquines;
                naveGanadora = datosNaves->idNave;
            } else if (puntuacion < disparosRecibidosSubcampeona - botiquinesSubcampeona || naveSubcampeona == -1){
                //Actualizamos solo la subcampeona
                disparosRecibidosSubcampeona = datosNaves->disparosRecibidos;
                disparosFalladosSubcampeona = datosNaves->disparosFallados;
                botiquinesSubcampeona = datosNaves->botiquines;
                naveSubcampeona = datosNaves->idNave;
            }
        }
        datosNaves = datosNaves->siguiente;
        numNavesJuzgadas++;
    }

    
    fprintf(salida, "Nave Ganadora: %d\n", naveGanadora); 
    fprintf(salida, "\tDisparos recibidos: %d\n", disparosRecibidosGanadora); 
    fprintf(salida, "\tDisparos fallados: %d\n", disparosFalladosGanadora); 
    fprintf(salida, "\tBotiquines obtenidos: %d\n", botiquinesGanadora); 
    fprintf(salida, "\tPuntuación: %d\n\n", disparosRecibidosGanadora - botiquinesGanadora); 

    if (naveSubcampeona != -1){ //Solo hay subcampeona si hay 2 que hayan leído tokens
        fprintf(salida, "Nave Subcampeona: %d\n", naveSubcampeona); 
        fprintf(salida, "\tDisparos recibidos: %d\n", disparosRecibidosSubcampeona); 
        fprintf(salida, "\tDisparos fallados: %d\n", disparosFalladosSubcampeona); 
        fprintf(salida, "\tBotiquines obtenidos: %d\n", botiquinesSubcampeona); 
        fprintf(salida, "\tPuntuación: %d\n\n", disparosRecibidosSubcampeona - botiquinesSubcampeona); 
    }
    
    fprintf(salida, "================== RESUMEN ==================\n"); 
    fprintf(salida, "\tDisparos recibidos Totales: %d\n", disparosRecibidosTotales); 
    fprintf(salida, "\tDisparos fallados Totales: %d\n", disparosFalladosTotales); 
    fprintf(salida, "\tBotiquines obtenidos Totales: %d\n", botiquinesEmitidosTotales); 
    fprintf(salida, "\tTotal de tokens emitidos: %d\n", disparosRecibidosTotales + disparosFalladosTotales + botiquinesEmitidosTotales); 
    fclose(salida);
    pthread_exit(0); 
}

int main(int argc, char* argv[]){
    
    if (argc != 5){
        fprintf(stderr, "NUMERO DE PARAMETROS INCORRECTO.");
        exit(-1);
    }

    //Obtener argumentos
    char* rutaEntrada = argv[1];
    char* rutaSalida = argv[2];
    int tamanoBuffer = atoi(argv[3]);
    if (tamanoBuffer <= 0){
        fprintf(stderr, "ERROR TAMAÑO DE BUFFER INVALIDO.");
        exit(-1);
    }
    int numNaves = atoi(argv[4]);
    if (numNaves <= 0){
        fprintf(stderr, "ERROR NUMERO DE NAVES INVALIDO.");
        exit(-1);
    }

    //Comprobamos el fichero de entrada
    FILE* fichero;
    if ((fichero = fopen(rutaEntrada, "r")) == NULL){
        fprintf(stderr, "ERROR AL ABRIR EL FICHERO DE ENTRADA.");
        exit(-1);
    }
    fclose(fichero);

    //Comprobamos el fichero de salida
    if ((fichero = fopen(rutaSalida, "w")) == NULL){
        fprintf(stderr, "ERROR AL ESCRIBIR EL FICHERO DE SALIDA.");
        exit(-1);
    }
    fclose(fichero);
    
    //Preparamos todos los hilos
    pthread_t* tid = (pthread_t*)malloc((numNaves+2)*sizeof(pthread_t)); //Disparador, juez y naves

    //Creamos el buffer
    char* buffer;
    if ((buffer = (char*) malloc(sizeof(char)*tamanoBuffer))==NULL){
        fprintf(stderr, "ERROR RESERVANDO MEMORIA.");
        exit(-1);
    }
    
    //Crear el disparador
    struct argDisp* argumentosDisparador;
    if ((argumentosDisparador = (struct argDisp*) malloc(sizeof(struct argDisp))) == NULL){
        fprintf(stderr, "ERROR RESERVANDO MEMORIA.");
        exit(-1);
    }
    argumentosDisparador->buffer = buffer;
    argumentosDisparador->rutaEntrada = rutaEntrada;
    argumentosDisparador->rutaSalida = rutaSalida;
    argumentosDisparador->tamanoBuffer = tamanoBuffer;
    sem_init(&bufferHayEspacio, 0, tamanoBuffer);
    sem_init(&bufferHayDato, 0, 0);
    sem_init(&disparadorFinaliza, 0, 0);
    pthread_create(&tid[0], NULL, disparador, (void*) argumentosDisparador);
    
    //Creación de la lista enlazada
    struct lista* datosNaves = (struct lista*)malloc(sizeof(struct lista));
    datosNaves->siguiente = NULL;

    //Crear las naves
    indiceLecturaBuffer = 0;
    sem_init(&mutexLista, 0, 1);
    sem_init(&naveFinaliza, 0, 0);
    sem_init(&mutexIndiceBuffer,0,1);
    for(int i = 0; i < numNaves; i++){
        struct argNave* argumentosNave;
        if ((argumentosNave = (struct argNave*)malloc(sizeof(struct argNave)))==NULL){
            fprintf(stderr, "ERROR RESERVANDO MEMORIA.");
            exit(-1);
        }
        argumentosNave->buffer = buffer;
        argumentosNave->lista = datosNaves;
        argumentosNave->id = i;
        argumentosNave->tamanoBuffer = tamanoBuffer;
        pthread_create(&tid[i+1], NULL, nave, (void*) argumentosNave);
    }

    //Crear el juez
    struct argJuez* argumentosJuez;
    if ((argumentosJuez = (struct argJuez*)malloc(sizeof(struct argJuez)))==NULL){
        fprintf(stderr, "ERROR RESERVANDO MEMORIA.");
        exit(-1);
    }
    argumentosJuez->rutaSalida = rutaSalida; 
    argumentosJuez->numNaves = numNaves;
    argumentosJuez->lista = datosNaves;
    pthread_create( &tid[numNaves+1], NULL, juez, (void*) argumentosJuez);
   
    //Esperamos a que terminen
    for (int i = 0; i < 2+numNaves; i++){
        pthread_join(tid[i], NULL); 
    }

    //Recogemos y nos vamos
    while (!datosNaves){
        struct lista* siguiente = datosNaves->siguiente;
        free(datosNaves);
        datosNaves = siguiente;
    }
    free(buffer);
    sem_destroy(&bufferHayEspacio);
    sem_destroy(&bufferHayDato);
    sem_destroy(&mutexIndiceBuffer);
    sem_destroy(&disparadorFinaliza);
    sem_destroy(&naveFinaliza);
    sem_destroy(&mutexLista);
    return 0;
}