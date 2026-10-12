# Ejercicio 2: Semáforo con Pulsador (MIPS32 / MARS)
# Simula un semáforo controlado por la tecla 's' utilizando temporizadores 
# precisos basados en el sistema (Syscall 30) y lectura MMIO.

.data
    msg_verde:    .asciiz "Semáforo en verde, esperando pulsador\n"
    msg_pulsado:  .asciiz "Pulsador activado: en 20 segundos, el semáforo cambiará a amarillo\n"
    msg_amarillo: .asciiz "Semáforo en amarillo, en 10 segundos, semáforo en rojo\n"
    msg_rojo:     .asciiz "Semáforo en rojo, en 30 segundos, semáforo en verde\n"
    
    # Direcciones MMIO para el teclado en MARS
    ADDR_RCV_CTRL: .word 0xffff0000
    ADDR_RCV_DATA: .word 0xffff0004

.text
.globl main

main:
    # Cargar direcciones base de MMIO
    lw $t8, ADDR_RCV_CTRL
    lw $t9, ADDR_RCV_DATA

ciclo_semaforo:
    # 1. Estado Verde: Mostrar mensaje y esperar pulsador 's'
    li $v0, 4
    la $a0, msg_verde
    syscall

esperar_tecla_s:
    # Verificar si hay una tecla lista en el puerto MMIO (sin bloquear el hilo de forma rígida)
    lw $t0, 0($t8)
    andi $t0, $t0, 1
    beqz $t0, esperar_tecla_s
    
    lw $t1, 0($t9)          # Leer el carácter ingresado
    li $t2, 115             # Código ASCII de la letra minúscula 's'
    bne $t1, $t2, esperar_tecla_s  # Si no es 's', seguir esperando

    # 2. Pulsador activado -> Mostrar mensaje e iniciar temporizador de 20 segundos[cite: 1]
    li $v0, 4
    la $a0, msg_pulsado
    syscall

    li $a0, 20000           # 20 segundos equivalen a 20000 milisegundos
    jal delay_ms

    # 3. Estado Amarillo -> Mostrar mensaje e iniciar temporizador de 10 segundos[cite: 1]
    li $v0, 4
    la $a0, msg_amarillo
    syscall

    li $a0, 10000           # 10 segundos = 10000 milisegundos
    jal delay_ms

    # 4. Estado Rojo -> Mostrar mensaje e iniciar temporizador de 30 segundos[cite: 1]
    li $v0, 4
    la $a0, msg_rojo
    syscall

    li $a0, 30000           # 30 segundos = 30000 milisegundos
    jal delay_ms

    j ciclo_semaforo        # Repetir el ciclo indefinidamente[cite: 1]


# Subrutina de retardo en milisegundos utilizando la Syscall 30 (Time)
# Recibe en $a0 el tiempo de espera deseado en milisegundos.
delay_ms:
    move $t3, $a0           # Guardar la duración objetivo
    
    # Obtener el tiempo inicial del sistema (milisegundos)
    li $v0, 30
    syscall                 # Retorna el tiempo en los registros $a0 (low) y $a1 (high)
    move $t4, $a0           # Almacenar el tiempo inicial en $t4

loop_delay:
    li $v0, 30
    syscall                 # Obtener el tiempo actual del sistema
    subu $t5, $a0, $t4      # Calcular tiempo transcurrido (actual - inicial)
    bltu $t5, $t3, loop_delay # Si el tiempo transcurrido es menor al deseado, repetir
    jr $ra                  # Retornar cuando el tiempo se haya cumplido