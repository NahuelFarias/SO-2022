#include "memoria.h"

// TODO: validar
void* escuchar_conexiones() {
  estado_conexion_memoria = true;
  char* ip = config_get_string_value(config, "IP_ESCUCHA");
  char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA");
  socket_memoria = iniciar_servidor(ip, puerto);

  while (estado_conexion_memoria) {
    int cliente_fd = esperar_cliente(socket_memoria);
    /*
        if (cliente_fd != -1) {
          t_paquete* paquete = paquete_create();
          t_buffer* mensaje = crear_mensaje("Conexión aceptada por MEMORIA");

          paquete_cambiar_mensaje(paquete, mensaje), enviar_mensaje(cliente_fd, paquete);
          // paquete_add_mensaje(paquete, mensaje);
        }
    */
    pthread_t th;
    pthread_create(&th, NULL, manejar_nueva_conexion, &cliente_fd), pthread_detach(th);
  }
  free(ip);
  free(puerto);
  pthread_exit(NULL);
}

// TODO: validar
void* manejar_nueva_conexion(void* args) {
  int socket_cliente = *(int*)args;
  estado_conexion_con_cliente = true;
  while (estado_conexion_con_cliente) {
    int codigo_operacion = recibir_operacion(socket_cliente);

    switch (codigo_operacion) {
      case MENSAJE_HANDSHAKE: {
        xlog(COLOR_CONEXION, "Handshake cpu - Se recibio solicitud handshake");
        t_paquete* paquete = recibir_paquete(socket_cliente);
        paquete_destroy(paquete);

        uint32_t entradas_por_tabla = 12; // config_get_int_value(config, "PAGINAS_POR_TABLA");
        uint32_t tam_pagina = 4;          // config_get_int_value(config, "TAM_PAGINA");
        t_mensaje_handshake_cpu_memoria* mensaje_handshake = mensaje_handshake_create(entradas_por_tabla, tam_pagina);

        t_paquete* paquete_con_respuesta = paquete_create();
        paquete_add_mensaje_handshake(paquete_con_respuesta, mensaje_handshake);
        enviar_mensaje_handshake(socket_cliente, paquete_con_respuesta);
        paquete_destroy(paquete_con_respuesta);

        break;
      }
      case OPERACION_MENSAJE: {
        recibir_mensaje(socket_cliente);
      } break;
      case OPERACION_EXIT: {
        xlog(COLOR_CONEXION, "Se recibió solicitud para finalizar ejecución");

        log_destroy(logger), close(socket_memoria);
        // TODO: no estaría funcionando del todo, queda bloqueado en esperar_cliente()
        estado_conexion_memoria = false;
        estado_conexion_con_cliente = false;

        break;
      }
      case OPERACION_OBTENER_SEGUNDA_TABLA: {
        xlog(COLOR_CONEXION, "Obteniendo numero de tabla de segundo nivel");
        t_paquete* paquete = recibir_paquete(socket_cliente);
        t_solicitud_segunda_tabla* solicitud_numero_tp_segundo_nivel = obtener_solicitud_tabla_segundo_nivel(paquete);

        int numero_TP_segundo_nivel = -1; // por defecto, en caso q no se encuentre

        int numero_tabla_primer_nivel = solicitud_numero_tp_segundo_nivel->num_tabla_primer_nivel;
        int entrada_primer_nivel = solicitud_numero_tp_segundo_nivel->entrada_primer_nivel;

        if (!dictionary_has_key(tablas_de_paginas_primer_nivel, string_itoa(numero_tabla_primer_nivel))) {
          numero_TP_segundo_nivel = -1;
        } else {
          t_tabla_primer_nivel* tabla_primer_nivel = dictionary_get(tablas_de_paginas_primer_nivel, string_itoa(numero_tabla_primer_nivel));

          if (!dictionary_has_key(tabla_primer_nivel->entradas_primer_nivel, string_itoa(entrada_primer_nivel))) {
            numero_TP_segundo_nivel = -1;
          } else {
            numero_TP_segundo_nivel = obtener_numero_TP_segundo_nivel(numero_tabla_primer_nivel, entrada_primer_nivel);
          }
        }

        // TODO: evaluar si el flujo de manejo de errores anteriores es correcto ò si falta considera otros casos
        // numero_TP_segundo_nivel = obtener_numero_TP_segundo_nivel(numero_tabla_primer_nivel, entrada_primer_nivel);

        // TODO: validar si no hay una función que agregue el contenido más fácil ó crear una abstracción
        t_paquete* paquete_respuesta = paquete_create();
        t_respuesta_solicitud_segunda_tabla* resp = malloc(sizeof(t_respuesta_solicitud_segunda_tabla));
        resp->socket = socket_cliente;
        resp->num_tabla_segundo_nivel = numero_TP_segundo_nivel;

        t_buffer* mensaje = crear_mensaje_respuesta_segunda_tabla(resp);
        paquete_cambiar_mensaje(paquete_respuesta, mensaje), enviar_operacion_respuesta_segunda_tabla(socket_cliente, paquete_respuesta);

        paquete_destroy(paquete_respuesta);
        paquete_destroy(paquete);

        break;
      }
      case OPERACION_OBTENER_MARCO: {
        xlog(COLOR_CONEXION, "Obteniendo numero de marco");
        t_paquete* paquete = recibir_paquete(socket_cliente);
        t_solicitud_marco* solicitud_numero_marco = malloc(sizeof(t_solicitud_marco));

        solicitud_numero_marco = obtener_solicitud_marco(paquete);

        int numero_tabla_segundo_nivel = solicitud_numero_marco->num_tabla_segundo_nivel;
        int entrada_segundo_nivel = solicitud_numero_marco->entrada_segundo_nivel;

        int num_marco = -1; // por defecto, en caso q no se encuentre

        // se entiende que si el marco es -1 entonces no se encontró,
        // el módulo que reciba esta respuesta debe interpretar lo anterior y manejarlo
        if (!dictionary_has_key(tablas_de_paginas_segundo_nivel, string_itoa(numero_tabla_segundo_nivel))) {
          num_marco = -1;
        } else {
          // TODO: no se está considerando el numero de TP de 1er nivel, debería???
          // tp_primer_nivel -> entrada_primer_nivel (referencia a la tp 2do nivel, y esta tiene entradas de 2do nivel)
          t_tabla_segundo_nivel* tabla_segundo_nivel = dictionary_get(tablas_de_paginas_segundo_nivel, string_itoa(numero_tabla_segundo_nivel));

          if (!dictionary_has_key(tabla_segundo_nivel->entradas_segundo_nivel, string_itoa(entrada_segundo_nivel))) {
            num_marco = -1;
          } else {
            num_marco = obtener_marco(solicitud_numero_marco->num_tabla_segundo_nivel, solicitud_numero_marco->entrada_segundo_nivel);
          }
        }

        // TODO: evaluar si el flujo de manejo de errores anteriores es correcto ò si falta considera otros casos
        // num_marco = obtener_marco(solicitud_numero_marco->num_tabla_segundo_nivel, solicitud_numero_marco->entrada_segundo_nivel);

        xlog(COLOR_INFO, "NUMERO MARCO: %d", num_marco);

        // TODO: validar si no hay una función que agregue el contenido más fácil ó crear una abstracción
        t_paquete* paquete_respuesta = paquete_create();
        t_respuesta_solicitud_marco* resp = malloc(sizeof(t_respuesta_solicitud_marco));
        resp->num_marco = num_marco;
        t_buffer* mensaje = crear_mensaje_respuesta_marco(resp);
        paquete_cambiar_mensaje(paquete_respuesta, mensaje), enviar_operacion_obtener_marco(socket_cliente, paquete_respuesta);

        paquete_destroy(paquete_respuesta);
        paquete_destroy(paquete);
        break;
      }
      case OPERACION_OBTENER_DATO: {
        xlog(COLOR_CONEXION, "Obteniendo dato fisico en memoria");
        t_paquete* paquete = recibir_paquete(socket_cliente);
        t_solicitud_dato_fisico* req = malloc(sizeof(t_solicitud_dato_fisico));
        req = obtener_solicitud_dato(paquete);

        uint32_t direccion_fisica = req->dir_fisica;
        free(req);

        uint32_t dato_buscado = buscar_dato_en_memoria(direccion_fisica);

        t_paquete* paquete_respuesta = paquete_create();
        t_respuesta_dato_fisico* resp = malloc(sizeof(t_respuesta_dato_fisico));
        memcpy(&(resp->dato_buscado), &dato_buscado, sizeof(uint32_t));
        t_buffer* mensaje = crear_mensaje_respuesta_dato_fisico(resp);
        paquete_cambiar_mensaje(paquete_respuesta, mensaje), enviar_operacion_obtener_dato(socket_cliente, paquete_respuesta);

        paquete_destroy(paquete_respuesta);

        break;
      }

      case OPERACION_ESCRIBIR_DATO: {
        xlog(COLOR_CONEXION, "Escribiendo dato en memoria");
        t_paquete* paquete = recibir_paquete(socket_cliente);
        t_escritura_dato_fisico* req = malloc(sizeof(t_escritura_dato_fisico));
        req = obtener_solicitud_escritura_dato(paquete);

        uint32_t direccion_fisica = req->dir_fisica;
        uint32_t valor = req->valor;
        free(req);

        uint32_t resultado_escritura = escribir_dato(direccion_fisica, valor);

        t_paquete* paquete_respuesta = paquete_create();
        t_respuesta_escritura_dato_fisico* resp = malloc(sizeof(t_respuesta_escritura_dato_fisico));
        resp->resultado = resultado_escritura;
        t_buffer* mensaje = crear_mensaje_respuesta_escritura_dato_fisico(resp);
        paquete_cambiar_mensaje(paquete_respuesta, mensaje), enviar_operacion_escribir_dato(socket_cliente, paquete_respuesta);

        paquete_destroy(paquete_respuesta);

        break;
      }
      case OPERACION_PROCESO_SUSPENDIDO: {
        t_paquete* paquete = recibir_paquete(socket_cliente);
        /*
        t_pcb* pcb = paquete_obtener_pcb(paquete);
        // TODO: resolver cuando se avance el módulo..
        // TODO: Escribir en swap paginas con bit M en 1

        t_list* marcos_asignados = obtener_marcos_asignados_a_este_proceso(pcb->pid);

        bool marco_modificado(t_marco * marco) {
          return marco->t_entrada_tabla_segundo_nivel->bit_modif == 1;
        }

        t_list* marcos_modificados = list_filter(marcos_asignados, (void*)marco_modificado);
        escribir_datos_de_marcos_en_swap(marcos_modificados);*/

        xlog(COLOR_CONEXION, "Se recibió solicitud de Kernel para suspender proceso");
        confirmar_suspension_de_proceso(socket_cliente, paquete);
        paquete_destroy(paquete);
      } break;
      case OPERACION_INICIALIZAR_ESTRUCTURAS: {
        t_paquete* paquete = recibir_paquete(socket_cliente);
        t_pcb* pcb = paquete_obtener_pcb(paquete);
        paquete_destroy(paquete);

        xlog(COLOR_CONEXION, "Se recibió solicitud de Kernel para inicializar estructuras de un proceso");

        // TODO: validar si no se está contemplando algo más aparte de agregar el numero de TP de 1º nivel al PCB
        int numero_tabla_primer_nivel = inicializar_estructuras_de_este_proceso(pcb->pid, pcb->tamanio);
        pcb->tabla_primer_nivel = numero_tabla_primer_nivel;
        t_paquete* paquete_con_pcb_actualizado = paquete_create();
        paquete_add_pcb(paquete_con_pcb_actualizado, pcb);

        confirmar_estructuras_en_memoria(socket_cliente, paquete_con_pcb_actualizado);
        paquete_destroy(paquete_con_pcb_actualizado);
      } break;
      case OPERACION_PROCESO_FINALIZADO: {
        t_paquete* paquete = recibir_paquete(socket_cliente);
        t_pcb* pcb = paquete_obtener_pcb(paquete);

        // TODO: falta validar con valgrind
        liberar_estructuras_en_memoria_de_este_proceso(pcb->pid);

        // TODO: resolver cuando se avance el módulo de swap
        liberar_estructuras_en_swap(pcb->pid);

        xlog(COLOR_CONEXION, "Memoria/Swap recibió solicitud de Kernel para liberar las estructuras de un proceso");

        pcb_destroy(pcb);
        paquete_destroy(paquete);
      } break;
      case -1: {
        log_info(logger, "el cliente se desconecto");
        estado_conexion_con_cliente = false;
        break;
      }
      default:
        log_warning(logger, "Operacion desconocida. No quieras meter la pata");
        break;
    }
  }
  pthread_exit(NULL);
}
