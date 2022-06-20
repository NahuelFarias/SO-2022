CLOCK_MODIFICADO_VICTIMA_NIVEL_PRIORIDAD obtener_prioridad_victima_segun_algoritmo_clock_modificado(t_entrada_tabla_segundo_nivel * entrada_elegida);
bool algoritmo_reemplazo_cargado_es(char * algoritmo);
bool es_victima_segun_algoritmo_clock(t_entrada_tabla_segundo_nivel * entrada_elegida);
bool es_victima_segun_algoritmo_clock_modificado(t_entrada_tabla_segundo_nivel * entrada_elegida);
bool hay_marcos_libres_asignados_al_proceso(int pid);
bool tiene_marco_asignado_entrada_TP(t_entrada_tabla_segundo_nivel * entrada);
char * obtener_algoritmo_reemplazo_por_config();
int asignar_marco_libre_o_reemplazar_pagina(int num_tabla_segundo_nivel,int entrada_segundo_nivel);
int cantidad_tablas_paginas_primer_nivel();
int escribir_dato(uint32_t dir_fisica,uint32_t valor);
int generar_numero_tabla();
int main();
int obtener_cantidad_entradas_por_tabla_por_config();
int obtener_cantidad_marcos_en_memoria();
int obtener_cantidad_marcos_por_proceso_por_config();
int obtener_marco(int numero_tabla_paginas_segundo_nivel,int numero_entrada_TP_segundo_nivel);
int obtener_numero_TP_segundo_nivel(int numero_TP_primer_nivel,int numero_entrada_TP_primer_nivel);
int obtener_pid_asignado_TP_segundo_nivel(int numero_entrada_TP_segundo_nivel);
int obtener_posicion_de_marco_del_listado(t_marco * marco,t_list * lista_marcos);
int obtener_tamanio_memoria_por_config();
int obtener_tamanio_pagina_por_config();
int obtener_y_asignar_marco_segun_algoritmo_de_reemplazo(int pid,t_entrada_tabla_segundo_nivel * entrada_segundo_nivel_solicitada_para_acceder);
int obtener_y_asignar_primer_marco_libre_asignado_al_proceso(int pid,t_entrada_tabla_segundo_nivel * entrada_TP_segundo_nivel);
t_entrada_tabla_segundo_nivel * ejecutar_clock(t_list * marcos_asignados,t_entrada_tabla_segundo_nivel * entrada_solicitada_para_acceder);
t_entrada_tabla_segundo_nivel * entrada_TP_segundo_nivel_create(int num_entrada,int num_marco,int bit_uso,int bit_modif,int bit_presencia);
t_entrada_tabla_segundo_nivel * entrada_victima_elegida_por_algoritmo_clock(t_list * marcos_asignados,t_entrada_tabla_segundo_nivel * entrada_solicitada_para_acceder);
t_entrada_tabla_segundo_nivel * entrada_victima_elegida_por_algoritmo_clock_modificado(t_list * marcos_asignados,t_entrada_tabla_segundo_nivel * entrada_solicitada_para_acceder);
t_entrada_tabla_segundo_nivel * obtener_entrada_tabla_segundo_nivel(int numero_entrada_TP_segundo_nivel);
t_list * obtener_marcos_asignados_a_este_proceso(int pid);
t_marco * algoritmo_clock_puntero_obtener_marco(t_list * lista_de_marcos);
t_tabla_primer_nivel * obtener_tabla_paginas_primer_nivel_por_pid(int pid);
t_tabla_primer_nivel * tabla_paginas_primer_nivel_create();
t_tabla_segundo_nivel * tabla_paginas_segundo_nivel_create(int numero_tabla_segundo_nivel);
uint32_t buscar_dato_en_memoria(uint32_t dir_fisica);
void * escuchar_conexiones();
void * manejar_nueva_conexion(void * args);
void * reservar_memoria_inicial(int size_memoria_total);
void algoritmo_clock_actualizar_puntero(t_marco * marco_seleccionado,t_marco * proximo_marco_seleccionado);
void dividir_memoria_principal_en_marcos();
void encontrar_marcos_en_tabla_segundo_nivel(int num_tabla_segundo_nivel,t_list * marcos);
void inicializar_entrada_de_tabla_paginas(t_entrada_tabla_segundo_nivel * entrada_tabla_segundo_nivel);
void inicializar_estructuras();
void inicializar_estructuras_de_este_proceso(int pid,int tam_proceso);
void liberar_estructuras_en_swap();
void llenar_memoria_mock();
void mostrar_tabla_marcos();
void reservar_marcos_mock();
