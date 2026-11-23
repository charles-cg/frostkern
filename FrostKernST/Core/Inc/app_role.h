#ifndef FROSTKERN_APP_ROLE_H
#define FROSTKERN_APP_ROLE_H

/* Definir exactamente UNO en los simbolos del compilador del IDE:
 * Master: ROLE_MASTER=1
 * Slave:  ROLE_SLAVE=1
 * No editar este archivo entre tarjetas. No hay un rol predeterminado.
 */
#if defined(ROLE_MASTER) == defined(ROLE_SLAVE)
#error "Define exactamente ROLE_MASTER=1 o ROLE_SLAVE=1"
#endif

#if defined(ROLE_MASTER)
#if ROLE_MASTER != 1
#error "ROLE_MASTER debe valer 1; para Slave elimina ROLE_MASTER"
#endif
#define APP_IS_MASTER 1
#define APP_IS_SLAVE  0
#else
#if ROLE_SLAVE != 1
#error "ROLE_SLAVE debe valer 1; para Master elimina ROLE_SLAVE"
#endif
#define APP_IS_MASTER 0
#define APP_IS_SLAVE  1
#endif

/* Este archivo NO selecciona la biblioteca Zigbee del linker ni configura
 * automaticamente coordinador/SED, tareas, perifericos o bajo consumo.
 * Esos cambios deben implementarse por perfil en el proyecto.
 */

#endif /* FROSTKERN_APP_ROLE_H */
