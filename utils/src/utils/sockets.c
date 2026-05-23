#include "sockets.h"

// Esto anda. mati. Da errores de compilacion el vs porque no es un archivo compilable pero funciona bien y 
// utiliza los metodos que expone el blog de ssoo en la parte de sockets en https://docs.utnso.com.ar/guias/linux/sockets

/// @brief Crea una conexión cliente hacia un servidor utilizando sockets
/// Esta función utiliza getaddrinfo() para obtener la información de red
/// necesaria y establece una conexión con el servidor especificado mediante
/// la dirección IP y el puerto
/// @param ip Dirección IP del servidor
/// @param puerto Puerto del servidor
/// @return Descriptor del socket si la conexión fue exitosa, -1 en caso de error
int crear_conexion(char* ip, char* puerto) {
    int error;
    // Estructuras utilizadas para obtener la información de red del servidor
    struct addrinfo hints, *server_info;

    // Inicializa la estructura hints en cero
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Obtiene la información necesaria para conectarse al servidor
    error = getaddrinfo(ip, puerto, &hints, &server_info);
    if (error != 0) {
        // manejo el error -> imprimo el puerto al que apuntaba
        fprintf(stderr, "Error creando conexion en getaddrinfo (IP: %s, Puerto: %s): %s\n",
                ip, puerto, gai_strerror(error));
        return -1;
    }

    // creo el socket
    int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

    // si hubo un error lo manejo
    if (socket_cliente == -1) {
        perror("Error al crear socket. Cerrando socket y liberando recursos");
        freeaddrinfo(server_info);
        printf("Memoria liberada correctamente despues de un error\n");
        return -1;
    }

    // Intenta establecer la conexión con el servidor y si falla lo manejo
    if (connect(socket_cliente,
                server_info->ai_addr,
                server_info->ai_addrlen) == -1) {
        perror("Error al conectar. Cerrando socket y liberando recursos");
        close(socket_cliente);
        freeaddrinfo(server_info);
        return -1;
    }

    // Libera la memoria reservada por getaddrinfo()
    freeaddrinfo(server_info);
    printf("Caso feliz: Memoria liberada correctamente\n");
    return socket_cliente;
}

/// @brief Inicia un servidor TCP que escucha conexiones entrantes
/// Configura y crea un socket servidor asociado al puerto indicado.
/// Utiliza setsockopt() para permitir la reutilización del puerto y evitar
/// errores al reiniciar la aplicación.
/// @param puerto Puerto en el cual el servidor escuchará conexiones
/// @return 
int iniciar_servidor(char* puerto) {
    int error;
    int opt = 1;
    // Estructuras utilizadas para obtener la información del servidor
    struct addrinfo hints, *servinfo;

    // Inicializa la estructura hints en cero
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Obtiene la información necesaria para crear al servidor
    error = getaddrinfo(NULL, puerto, &hints, &servinfo);
    if (error != 0) {
        // manejo el error -> imprimo el puerto al que apuntaba
        fprintf(stderr, "Error iniciando servidor en getaddrinfo (Puerto: %s): %s\n",
                puerto, gai_strerror(error));
        return -1;
    }

    // creo el socket
    int socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    // si hubo un error lo manejo
    if (socket_servidor == -1) {
        perror("Error al crear socket. Cerrando socket y liberando recursos");
        freeaddrinfo(servinfo);
        printf("Memoria liberada correctamente despues de un error\n");
        return -1;
    }

    // Permite reutilizar el puerto inmediatamente después de cerrar la aplicación
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Asocia el socket al puerto especificado y si falla lo manejo
    if (bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("Error en bind. Cerrando socket y liberando recursos");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        printf("Memoria liberada correctamente despues de un error\n");
        return -1;
    }
    
    // Coloca al servidor en modo escucha para aceptar conexiones entrantes y si falla lo manejo
    if (listen(socket_servidor, SOMAXCONN) == -1) {
        perror("Error en listen. Cerrando socket y liberando recursos");
        close(socket_servidor);
        freeaddrinfo(servinfo);
        printf("Memoria liberada correctamente despues de un error\n");
        return -1;
    }

    // Libera la memoria reservada por getaddrinfo()
    freeaddrinfo(servinfo);
    printf("Caso feliz: Memoria liberada correctamente\n");
    return socket_servidor;
}



/// @brief Espera la conexión de un cliente al servidor.
/// Esta función bloquea la ejecución hasta que un cliente se conecta.
/// @param socket_servidor Descriptor del socket servidor
/// @return int Descriptor del socket del cliente conectado, o -1 en caso de error.
int esperar_cliente(int socket_servidor) {
    int socket_cliente = accept(socket_servidor, NULL, NULL);
    if (socket_cliente == -1) {
        perror("Error al accept. Cerrando socket y liberando recursos");
    }
    return socket_cliente;
}

/// @brief Cierra una conexión de socket
/// @param socket_cliente Descriptor del socket a cerrar
void liberar_conexion(int socket_cliente) {
    close(socket_cliente);
}