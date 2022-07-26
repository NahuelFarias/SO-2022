#include "cpu.h"
#include "mmu.h"

int main() {
  HAY_PCB_PARA_EJECUTAR_ = 0;
  HAY_INTERRUPCION_ = 0;

  logger = iniciar_logger(DIR_LOG_MESSAGES, "CPU");
  config = iniciar_config(DIR_CPU_CFG);

  iniciar_tlb();

  // estado_conexion_con_cliente = false;

  pthread_t th, th1, th2;
  pthread_create(&th, NULL, (void*)escuchar_dispatch_, NULL), pthread_detach(th);
  pthread_create(&th1, NULL, (void*)realizar_handshake_memoria, NULL), pthread_detach(th1);
  pthread_create(&th2, NULL, (void*)iniciar_conexion_interrupt, NULL), pthread_detach(th2);

  // PARA PRUEBAS - COMENTAR SI NO SE UTILIZA
  // pthread_t th_test;
  // pthread_create(&th_test, NULL, (void*)prueba_comunicacion_memoria, NULL), pthread_detach(th_test);

  xlog(COLOR_INFO, "CPU - Servidor listo para recibir al cliente Kernel");

  pthread_exit(0);
}

void realizar_handshake_memoria() {
  xlog(COLOR_INFO, "Inicio handshake con memoria");

  t_paquete* paquete = paquete_create();
  t_buffer* buffer = crear_mensaje("Handshake CPU - MEMORIA");
  paquete->buffer = buffer;

  socket_memoria = conectarse_a_memoria();
  enviar_mensaje_handshake(socket_memoria, paquete);

  // int cliente_fd = esperar_cliente(socket_memoria);
  esperar_cliente(socket_memoria);

  int cod_op = recibir_operacion(socket_memoria);

  switch (cod_op) {
    case MENSAJE_HANDSHAKE: {
      xlog(COLOR_CONEXION, "Handshake memoria - se recibio la respuesta");
      t_paquete* paquete_respuesta = malloc(sizeof(t_paquete) + 1);
      paquete_respuesta = recibir_paquete(socket_memoria);
      t_mensaje_handshake_cpu_memoria* handshake_deserializado = paquete_obtener_mensaje_handshake(paquete_respuesta);
      tamanio_pagina = handshake_deserializado->tamanio_pagina;
      entradas_por_tabla = handshake_deserializado->entradas_por_tabla;
      xlog(COLOR_INFO,
           "Handshake memoria - respuesta -> entradas por tabla: %d, tamanio pagina: %d",
           handshake_deserializado->entradas_por_tabla,
           handshake_deserializado->tamanio_pagina);
      paquete_destroy(paquete_respuesta);
      break;
    }
    case -1: {
      xlog(COLOR_CONEXION, "Handshake memoria - el cliente se desconecto");
      // cliente_estado = CLIENTE_EXIT;
      estado_conexion_con_cliente = false;
      break;
    }
    default: {
      xlog(COLOR_ERROR, "Operacion desconocida.");
      break;
    }
  }

  paquete_destroy(paquete);

  pthread_exit(NULL);
}

int conectarse_a_memoria() {
  char* ip = config_get_string_value(config, "IP_MEMORIA");
  char* puerto = config_get_string_value(config, "PUERTO_MEMORIA");
  int fd_servidor = conectar_a_servidor(ip, puerto);

  if (fd_servidor == -1) {
    log_error(logger, "No se pudo establecer la conexión con MEMORIA, inicie el servidor con %s e intente nuevamente", puerto);

    return -1;
  } else {
    xlog(COLOR_CONEXION, "Se conectó con éxito a Memoria a través de la conexión %s", puerto);
  }

  free(ip);
  free(puerto);

  return fd_servidor;
}

void* escuchar_dispatch_() {
  estado_conexion_kernel = true;

  char* ip = config_get_string_value(config, "IP_ESCUCHA");
  char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
  socket_cpu_dispatch = iniciar_servidor(ip, puerto);

  while (estado_conexion_kernel) {
    int cliente_fd = esperar_cliente(socket_cpu_dispatch);

    pthread_t th;
    pthread_create(&th, NULL, manejar_nueva_conexion_, &cliente_fd), pthread_detach(th);
  }

  free(ip);
  free(puerto);

  pthread_exit(NULL);
}

void* manejar_nueva_conexion_(void* args) {
  int socket_cliente = *(int*)args;

  estado_conexion_con_cliente = true;

  while (estado_conexion_con_cliente) {
    uint32_t cod_op = recibir_operacion(socket_cliente);

    switch (cod_op) {
      case OPERACION_PCB: {
        xlog(COLOR_PAQUETE, "CPU - Iniciando una nueva operacion pcb");
        t_paquete* paquete_con_pcb = malloc(sizeof(t_paquete) + 1);
        paquete_con_pcb = recibir_paquete(socket_cliente);

        t_pcb* pcb_deserializado = paquete_obtener_pcb(paquete_con_pcb);

        // TODO: hay que pensarlo bien esta variable porque en la teoria esta mal
        HAY_PCB_PARA_EJECUTAR_ = 1;
        ciclo_instruccion(pcb_deserializado, socket_cliente);
        imprimir_pcb(pcb_deserializado);
        paquete_destroy(paquete_con_pcb);
        break;
      }
      case OPERACION_EXIT: {
        xlog(COLOR_CONEXION, "Se recibió solicitud para finalizar ejecución");

        log_destroy(logger), close(socket_cpu_dispatch);
        // TODO: no estaría funcionando del todo, queda bloqueado en esperar_cliente()
        estado_conexion_kernel = false;
        estado_conexion_con_cliente = false;
        break;
      }
      case -1: {
        xlog(COLOR_CONEXION, "el cliente se desconecto");
        // cliente_estado = CLIENTE_EXIT;
        estado_conexion_con_cliente = false;
        break;
      }
      default: {
        xlog(COLOR_ERROR, "Operacion desconocida.");
        break;
      }
    }
  }
  pthread_exit(NULL);
}

void ciclo_instruccion(t_pcb* pcb, uint32_t socket_cliente) {
  xlog(COLOR_INFO, "Iniciando ciclo de instruccion, pcbid: %d", pcb->pid);

  limpiar_tlb(pcb->pid);

  while (HAY_PCB_PARA_EJECUTAR_ && pcb->program_counter < list_size(pcb->instrucciones)) {
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    instruccion = fetch(pcb);
    pcb->program_counter++;

    uint32_t fetch_operands_necesaria = decode(instruccion);

    int dato_leido_copy = 0;
    if (fetch_operands_necesaria == 0) {
      dato_leido_copy = fetch_operands(pcb, instruccion);
    }

    if (dato_leido_copy < 0) {
      xlog(COLOR_ERROR, "CPU - no se realiza el EXECUTE ya que ocurrio un error con el fetch operands");
    } else {
      execute(pcb, instruccion, socket_cliente, dato_leido_copy);
    }

    check_interrupt(pcb, socket_cliente);
  }

  xlog(COLOR_INFO, "Finalizando ciclo de instruccion, pcbid: %d", pcb->pid);
}

t_instruccion* fetch(t_pcb* pcb) {
  xlog(COLOR_INFO, "Realizando fetch pcb id: %d", pcb->pid);
  return list_get(pcb->instrucciones, pcb->program_counter);
}

uint32_t decode(t_instruccion* instruccion) {
  xlog(COLOR_INFO, "Realizando decode instruccion: %s", instruccion->identificador);
  return (strcmp(instruccion->identificador, "COPY"));
}

int fetch_operands(t_pcb* pcb, t_instruccion* instruccion) {
  int direccion_fisica = obtener_direccion_fisica_memoria(pcb, instruccion, 1); // parametro 1

  if (direccion_fisica < 0) {
    xlog(COLOR_ERROR, "FETCH OPERANDS - Error al intentar obtener datos de memoria. Direccion fisica negativa.");
    return direccion_fisica;
  }
  uint32_t dato_leido = obtener_dato_fisico(direccion_fisica);

  xlog(COLOR_TAREA, "FETCH OPERANDS - (COPY) Dato leido a copiar: %d", dato_leido);

  return dato_leido;
}

void execute(t_pcb* pcb, t_instruccion* instruccion, uint32_t socket_cliente, uint32_t dato_leido_copy) {
  xlog(COLOR_INFO, "Realizando execute pcb id: %d", pcb->pid);

  if (strcmp(instruccion->identificador, "NO_OP") == 0) {
    xlog(COLOR_INFO, "Ejecutando NO_OP, pcb id: %d", pcb->pid);
    // uint32_t cantidad_de_veces_no_op = instruccion_obtener_parametro(instruccion, 0); // no sirve ya que desde
    // consola lo separa en inst individuales
    execute_no_op();

  } else if (strcmp(instruccion->identificador, "I/O") == 0) {
    xlog(COLOR_INFO, "Ejecutando I/O, pcb id: %d", pcb->pid);
    execute_io(pcb, instruccion, socket_cliente);

  } else if (strcmp(instruccion->identificador, "READ") == 0) {
    xlog(COLOR_INFO, "Ejecutando READ, pcb id: %d", pcb->pid);
    execute_read(pcb, instruccion);

  } else if (strcmp(instruccion->identificador, "WRITE") == 0) {
    xlog(COLOR_INFO, "Ejecutando WRITE, pcb id: %d", pcb->pid);
    execute_write(pcb, instruccion);

  } else if (strcmp(instruccion->identificador, "COPY") == 0) {
    xlog(COLOR_INFO, "Ejecutando COPY, pcb id: %d", pcb->pid);
    execute_copy(pcb, instruccion, dato_leido_copy);

  } else if (strcmp(instruccion->identificador, "EXIT") == 0) {
    xlog(COLOR_CONEXION, "Ejecutando EXIT, pcb id: %d", pcb->pid);
    execute_exit(pcb, socket_cliente);
  }
}

void check_interrupt(t_pcb* pcb, uint32_t socket_cliente) {
  if (HAY_PCB_PARA_EJECUTAR_) {
    xlog(COLOR_INFO, "Realizando check interrupt");
    if (HAY_INTERRUPCION_) {
      t_paquete* paquete = paquete_create();
      paquete_add_pcb(paquete, pcb);
      enviar_pcb_desalojado(socket_cliente, paquete);
      xlog(COLOR_TAREA, "Interrupcion - Se ha desalojado un PCB de CPU (pcb=%d)", pcb->pid);
      HAY_PCB_PARA_EJECUTAR_ = 0;
      HAY_INTERRUPCION_ = 0;
    }
  } else {
    HAY_INTERRUPCION_ = 0; // Para el caso en el que no haya pcb pero se haya mandado una interrupcion
  }
}

void execute_no_op() {
  int retardo = config_get_int_value(config, "RETARDO_NOOP");
  xlog(COLOR_TAREA, "Retardo de NO_OP en milisegundos: %d", retardo);
  usleep(retardo * 1000);
}

void execute_io(t_pcb* pcb, t_instruccion* instruccion, uint32_t socket_cliente) {
  uint32_t tiempo_bloqueado = instruccion_obtener_parametro(instruccion, 0);
  pcb->tiempo_de_bloqueado = tiempo_bloqueado;

  t_paquete* paquete = paquete_create();
  paquete_add_pcb(paquete, pcb);
  xlog(COLOR_TAREA, "Se actualizó el tiempo de bloqueo de un proceso (pid=%d, tiempo=%d)", pcb->pid, tiempo_bloqueado);
  enviar_pcb_con_operacion_io(socket_cliente, paquete);
  HAY_PCB_PARA_EJECUTAR_ = 0;
}

void execute_read(t_pcb* pcb, t_instruccion* instruccion) {
  int direccion_fisica = obtener_direccion_fisica_memoria(pcb, instruccion, 0); // parametro 0

  if (direccion_fisica < 0) {
    xlog(COLOR_ERROR, "OPERACION READ - Error al intentar obtener datos de memoria. Direccion fisica negativa.");
    return;
  }

  uint32_t dato_leido = obtener_dato_fisico(direccion_fisica);

  xlog(COLOR_TAREA, "OPERACION READ - Dato leido: %d.", dato_leido);
}

void execute_write(t_pcb* pcb, t_instruccion* instruccion) {
  int direccion_fisica = obtener_direccion_fisica_memoria(pcb, instruccion, 0); // parametro 0

  if (direccion_fisica < 0) {
    xlog(COLOR_ERROR, "OPERACION WRITE - Error al intentar obtener datos de memoria. Direccion fisica negativa.");
    return;
  }

  uint32_t dato_a_escribir = instruccion_obtener_parametro(instruccion, 1);

  uint32_t resultado = escribir_dato_memoria(direccion_fisica, dato_a_escribir);

  if (resultado == 1) {
    xlog(COLOR_TAREA, "OPERACION WRITE - Dato escrito correctamente. Dato escrito: %d, DF: %d.", dato_a_escribir, direccion_fisica);
  } else {
    xlog(COLOR_ERROR, "OPERACION WRITE - Error al escribir dato.");
  }
}

void execute_copy(t_pcb* pcb, t_instruccion* instruccion, uint32_t dato_a_escribir) {
  int direccion_fisica = obtener_direccion_fisica_memoria(pcb, instruccion, 0); // parametro 0

  if (direccion_fisica < 0) {
    xlog(COLOR_ERROR, "OPERACION COPY - Error al intentar obtener datos de memoria. Direccion fisica negativa.");
    return;
  }

  uint32_t resultado = escribir_dato_memoria(direccion_fisica, dato_a_escribir);

  if (resultado == 1) {
    xlog(COLOR_TAREA, "OPERACION COPY - Dato copiado correctamente. Dato escrito: %d, DF: %d.", dato_a_escribir, direccion_fisica);
  } else {
    xlog(COLOR_ERROR, "OPERACION COPY - Error al copiar dato.");
  }
}

void execute_exit(t_pcb* pcb, int socket_cliente) {
  t_paquete* paquete = paquete_create();
  paquete_add_pcb(paquete, pcb);
  enviar_pcb_con_operacion_exit(socket_cliente, paquete);
  HAY_PCB_PARA_EJECUTAR_ = 0;
}

uint32_t escribir_dato_memoria(uint32_t direccion_fisica, uint32_t dato_a_escribir) {
  t_escritura_dato_fisico* solicitud = malloc(sizeof(t_escritura_dato_fisico));

  solicitud->socket = socket_memoria;
  solicitud->dir_fisica = direccion_fisica;
  solicitud->valor = dato_a_escribir;

  t_paquete* paquete = paquete_create();
  t_buffer* mensaje = crear_mensaje_escritura_dato_fisico(solicitud);
  paquete_cambiar_mensaje(paquete, mensaje);
  enviar_operacion_escribir_dato(socket_memoria, paquete);

  free(paquete);
  free(mensaje);
  free(solicitud);

  // RECIBO RESPUESTA DE MEMORIA
  recibir_operacion(socket_memoria);
  t_paquete* paquete_respuesta = recibir_paquete(socket_memoria);
  t_respuesta_escritura_dato_fisico* respuesta = obtener_respuesta_escritura_dato_fisico(paquete_respuesta);
  uint32_t retorno = respuesta->resultado;

  free(paquete_respuesta);
  free(respuesta);

  return retorno;
}

int obtener_direccion_fisica_memoria(t_pcb* pcb, t_instruccion* instruccion, uint32_t numero_parametro) {
  uint32_t direccion_logica = instruccion_obtener_parametro(instruccion, numero_parametro);
  uint32_t numero_pagina = obtener_numero_pagina(direccion_logica);
  int existe_pagina = existe_pagina_en_tlb(numero_pagina);
  int marco;

  if (existe_pagina == -1) { // Si no esta la pagina en la tlb
    marco = obtener_marco_memoria(pcb->tabla_primer_nivel, numero_pagina);
  } else {
    marco = obtener_marco_tlb(existe_pagina);
  }

  if (marco < 0) {
    xlog(COLOR_ERROR, "Ocurrio un error al consultar el marco a memoria. MMU no calcula la direccion fisica");
    return marco;
  }


  agregar_pagina_marco_tlb(numero_pagina, marco, pcb->pid);

  uint32_t desplazamiento = obtener_desplazamiento(direccion_logica, numero_pagina);
  uint32_t direccion_fisica = obtener_direccion_fisica(desplazamiento, marco);

  return direccion_fisica;
}

int obtener_marco_memoria(uint32_t tabla_primer_nivel, uint32_t numero_pagina) {
  uint32_t entrada_primer_nivel = obtener_entrada_1er_nivel(numero_pagina, entradas_por_tabla);
  int tabla_segundo_nivel = obtener_tabla_segundo_nivel(tabla_primer_nivel, entrada_primer_nivel);

  if (tabla_segundo_nivel < 0) {
    xlog(COLOR_ERROR, "Ocurrio un error al consultar la tabla de segundo nivel a memoria. Respuesta: %d", tabla_segundo_nivel);
    return tabla_segundo_nivel;
  }

  xlog(COLOR_INFO, "TABLA SEGUNDO NIVEL obtenida de memoria: %d", tabla_segundo_nivel);

  uint32_t entrada_segundo_nivel = obtener_entrada_2do_nivel(numero_pagina, entradas_por_tabla);
  int marco = obtener_marco(tabla_segundo_nivel, entrada_segundo_nivel);

  if (marco < 0) {
    xlog(COLOR_ERROR, "Ocurrio un error al consultar el marco a memoria. Respuesta: %d", marco);
    return marco;
  }

  xlog(COLOR_INFO, "MARCO obtenido de memoria: %d", marco);

  return marco;
}

uint32_t obtener_dato_fisico(uint32_t direccion_fisica) {
  t_solicitud_dato_fisico* solicitud = malloc(sizeof(t_solicitud_dato_fisico));

  solicitud->socket = socket_memoria;
  solicitud->dir_fisica = direccion_fisica;

  t_paquete* paquete = paquete_create();
  t_buffer* mensaje = crear_mensaje_obtener_dato_fisico(solicitud);
  paquete_cambiar_mensaje(paquete, mensaje);
  enviar_operacion_obtener_dato(socket_memoria, paquete);

  free(paquete);
  free(mensaje);
  free(solicitud);

  // RECIBO RESPUESTA DE MEMORIA
  recibir_operacion(socket_memoria);
  t_paquete* paquete_respuesta = recibir_paquete(socket_memoria);
  t_respuesta_dato_fisico* respuesta = obtener_respuesta_solicitud_dato_fisico(paquete_respuesta);
  uint32_t retorno = respuesta->dato_buscado;

  free(paquete_respuesta);
  free(respuesta);

  return retorno;
}

int obtener_marco(int tabla_segundo_nivel, uint32_t entrada_segundo_nivel) {
  t_solicitud_marco* solicitud = malloc(sizeof(t_solicitud_marco));

  solicitud->socket = socket_memoria;
  solicitud->num_tabla_segundo_nivel = tabla_segundo_nivel;
  solicitud->entrada_segundo_nivel = entrada_segundo_nivel;

  t_paquete* paquete = paquete_create();
  t_buffer* mensaje = crear_mensaje_obtener_marco(solicitud);
  paquete_cambiar_mensaje(paquete, mensaje);
  enviar_operacion_obtener_marco(socket_memoria, paquete);

  free(paquete);
  free(mensaje);
  free(solicitud);

  // RECIBO RESPUESTA DE MEMORIA
  recibir_operacion(socket_memoria);
  t_paquete* paquete_respuesta = recibir_paquete(socket_memoria);
  t_respuesta_solicitud_marco* respuesta = obtener_respuesta_solicitud_marco(paquete_respuesta);
  int retorno = respuesta->num_marco;

  free(paquete_respuesta);
  free(respuesta);

  return retorno;
}

int obtener_tabla_segundo_nivel(uint32_t tabla_primer_nivel, uint32_t entrada_primer_nivel) {
  t_solicitud_segunda_tabla* solicitud = malloc(sizeof(t_solicitud_segunda_tabla));

  solicitud->socket = socket_memoria;
  solicitud->num_tabla_primer_nivel = tabla_primer_nivel;
  solicitud->entrada_primer_nivel = entrada_primer_nivel;

  t_paquete* paquete = paquete_create();
  t_buffer* mensaje = crear_mensaje_obtener_segunda_tabla(solicitud);
  paquete_cambiar_mensaje(paquete, mensaje);
  enviar_operacion_obtener_segunda_tabla(socket_memoria, paquete);

  free(paquete);
  free(mensaje);
  free(solicitud);

  // RECIBO RESPUESTA DE MEMORIA
  recibir_operacion(socket_memoria);
  t_paquete* paquete_respuesta = recibir_paquete(socket_memoria);
  t_respuesta_solicitud_segunda_tabla* respuesta_solicitud = obtener_respuesta_solicitud_tabla_segundo_nivel(paquete_respuesta);
  int retorno = respuesta_solicitud->num_tabla_segundo_nivel;

  free(paquete_respuesta);
  free(respuesta_solicitud);

  return retorno;
}

uint32_t instruccion_obtener_parametro(t_instruccion* instruccion, uint32_t numero_parametro) {
  char** parametros = string_split(instruccion->params, " ");
  uint32_t valor = atoi(parametros[numero_parametro]);

  string_iterate_lines(parametros, (void*)free);

  return valor;
}

void* iniciar_conexion_interrupt() {
  char* ip = config_get_string_value(config, "IP_ESCUCHA");
  char* puerto = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");

  CONEXION_CPU_INTERRUPT = iniciar_servidor(ip, puerto); // TODO: evaluar posibilidad de condición de carrera

  xlog(COLOR_CONEXION, "Conexión Interrupt lista con el cliente Kernel");

  pthread_t th;
  pthread_create(&th, NULL, (void*)escuchar_conexiones_entrantes_en_interrupt, NULL), pthread_detach(th);

  pthread_exit(NULL);
}

void* escuchar_conexiones_entrantes_en_interrupt() {
  CONEXION_ESTADO estado_conexion_con_cliente = CONEXION_ESCUCHANDO;
  CONEXION_ESTADO ESTADO_CONEXION_INTERRUPT = CONEXION_ESCUCHANDO;

  while (ESTADO_CONEXION_INTERRUPT) {
    uint32_t socket_cliente = esperar_cliente(CONEXION_CPU_INTERRUPT);
    estado_conexion_con_cliente = CONEXION_ESCUCHANDO;

    while (estado_conexion_con_cliente) {
      uint32_t codigo_operacion = recibir_operacion(socket_cliente);

      switch (codigo_operacion) {
        case OPERACION_INTERRUPT: {
          t_paquete* paquete = recibir_paquete(socket_cliente);
          xlog(COLOR_PAQUETE, "se recibió una Interrupción");

          HAY_INTERRUPCION_ = 1;
          paquete_destroy(paquete);
        } break;
        case OPERACION_MENSAJE: {
          recibir_mensaje(socket_cliente);
        } break;
        case OPERACION_EXIT: {
          xlog(COLOR_CONEXION, "Se recibió solicitud para finalizar ejecución");

          log_destroy(logger), close(CONEXION_CPU_INTERRUPT);
          // TODO: no estaría funcionando del todo, queda bloqueado en esperar_cliente()
          ESTADO_CONEXION_INTERRUPT = CONEXION_FINALIZADA;
          estado_conexion_con_cliente = CONEXION_FINALIZADA;
        } break;
        case -1: {
          xlog(COLOR_CONEXION, "el cliente se desconecto (socket=%d)", socket_cliente);

          // centinela para detener el loop del hilo asociado a la conexión
          // entrante
          estado_conexion_con_cliente = CONEXION_FINALIZADA;
        } break;
        default: { xlog(COLOR_ERROR, "Operacion %d desconocida", codigo_operacion); } break;
      }
    }
  }

  pthread_exit(NULL);
}

void prueba_comunicacion_memoria() {
  // t_instruccion* instruccion = malloc(sizeof(t_instruccion));
  // instruccion->identificador = "WRITE";
  // instruccion->params = "96 42";
  // t_pcb* pcb = malloc(sizeof(t_pcb));
  // pcb->tabla_primer_nivel = 4;
  // execute_write(pcb, instruccion);

  // t_instruccion* instruccion2 = malloc(sizeof(t_instruccion));
  // instruccion2->identificador = "READ";
  // instruccion2->params = "96";
  // execute_read(pcb, instruccion2);

  escribir_dato_memoria(96, 5000);

  obtener_dato_fisico(96);
}