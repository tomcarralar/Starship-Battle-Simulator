# Starship-Battle-Simulator

## Descripción
Esta práctica tiene como objetivo desarrollar un sistema concurrente en C que simula una batalla entre naves. El sistema procesa tokens provenientes de un archivo de texto, que representan disparos, fallos y botiquines obtenidos por las naves. Utilizando hilos, memoria compartida y semáforos, las naves leerán tokens desde un buffer circular y se gestionarán los resultados para determinar la nave ganadora.

## Objetivos de la práctica
- Crear y gestionar hilos en UNIX.
- Sincronizar el funcionamiento de hilos.
- Implementar listas enlazadas con memoria dinámica.
- Gestionar acceso a memoria compartida utilizando semáforos.

## Estructura del programa
El programa se compone de los siguientes componentes:

- Disparador:
    - Lee los tokens del archivo de entrada y los coloca en el buffer circular.
    - Filtra los tokens válidos e inválidos, almacenando estadísticas.

- Naves:

  - Cada nave lee los tokens del buffer circular, contando disparos recibidos, fallados y botiquines obtenidos.
  - Los resultados se guardan en una lista enlazada.
 
- Juez:

- Recoge los resultados de las naves y determina el ganador y subcampeón.
- Escribe los resultados finales en el archivo de salida.

## Requisitos del sistema
- Lenguaje: C estándar.

## Parámetros de entrada
El programa recibe cuatro parámetros:
- "./programName inputFile outputFile tamBuffer numNaves"
    - inputFile: Nombre del archivo de entrada que contiene los tokens.
    - outputFile: Archivo donde se escriben los resultados.
    - tamBuffer: Tamaño del buffer circular.
    - numNaves: Número de naves.
