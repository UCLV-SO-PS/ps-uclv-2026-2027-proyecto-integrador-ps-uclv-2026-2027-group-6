#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Protocolo TCP del monitor de calificaciones IMDb.
 *
 * Solicitud:
 *   El cliente envia EXACTAMENTE EPISODE_CODE_LEN (6) bytes con el codigo
 *   del episodio (por ejemplo "S01E01"). No se envia salto de linea ni
 *   terminador '\0'.
 *
 * Respuesta:
 *   Texto legible terminado en '\n'. El servidor CIERRA la conexion al
 *   terminar de enviar la respuesta; el cliente lee hasta EOF. No se
 *   reutiliza la misma conexion para varias consultas.
 *
 *   Episodio encontrado ("OK" seguido del bloque formateado por la
 *   biblioteca):
 *       OK
 *       ================================
 *              IMDb EPISODE MONITOR
 *       ================================
 *       Codigo:        S01E01
 *       Episodio:      The Hedge Knight
 *       Calificacion:  9.2 / 10
 *       Votos:         45120
 *
 *   Episodio inexistente:
 *       ERROR
 *       Codigo de episodio no encontrado.
 */

#define MONITOR_PORT 6666

#define EPISODE_CODE_LEN 6
#define EPISODE_CODE_STR_SIZE (EPISODE_CODE_LEN + 1)

#define RESPONSE_STATUS_OK "OK"
#define RESPONSE_STATUS_ERROR "ERROR"

#endif
