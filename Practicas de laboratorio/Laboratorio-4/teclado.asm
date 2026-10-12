# Ejercicio 1: Buffer de Teclado con Filtro de Caracteres (MIPS32 / MARS)
# Permite la entrada por teclado durante un intervalo de tiempo, filtrando 
# únicamente letras mayúsculas (A-Z) en un buffer circular, y las imprime al expirar.

.data
    buffer:       .space 100          # Buffer circular de 100 bytes
    buffer_size:  .word 100
    msg_print:    .asciiz "\nContenido del buffer:\n"
    msg_nl:       .asciiz "\n"
    
    # Direcciones MMIO para MARS Keyboard Receiver
    ADDR_RCV_CTRL: .word 0xffff0000
    ADDR_RCV_DATA: .word 0xffff0004

.text
.globl main

main:
    # Cargar direcciones base de MMIO
    lw $t8, ADDR_RCV_CTRL
    lw $t9, ADDR_RCV_DATA

main_loop:
    # Inicializar punteros del buffer circular
    # $s0: cabeza (head - escritura), $s1: cola (tail - lectura)
    li $s0, 0   # head index
    li $s1, 0   # tail index
    
    # Configurar contador de tiempo (aproximación para el bucle de espera)
    li $s2, 1500000  # Ajustar según la velocidad de simulación del entorno MARS

timer_loop:
    # Verificar si hay un carácter disponible en el teclado (Polling no bloqueante)
    lw $t0, 0($t8)
    andi $t0, $t0, 1        # Bit 0 (ready bit)
    beqz $t0, check_time    # Si no hay tecla lista, saltar a verificar el tiempo

    # Leer el carácter del registro de datos
    lw $t1, 0($t9)

    # Filtrar solo letras mayúsculas (A-Z: código ASCII 65 a 90)
    li $t2, 65              # 'A'
    li $t3, 90              # 'Z'
    blt $t1, $t2, check_time  # Menor que 'A', descartar
    bgt $t1, $t3, check_time  # Mayor que 'Z', descartar

    # Almacenar en el buffer circular
    la $t4, buffer
    add $t4, $t4, $s0       # Dirección base + offset de cabeza
    sb $t1, 0($t4)          # Guardar byte en memoria

    # Actualizar puntero de cabeza (head) de forma circular
    addi $s0, $s0, 1
    lw $t5, buffer_size
    rem $s0, $s0, $t5       # head = (head + 1) % buffer_size

check_time:
    # Decrementar contador del temporizador
    subi $s2, $s2, 1
    bnez $s2, timer_loop    # Si el tiempo no ha expirado, continuar la captura

    # Transcurrido el tiempo, imprimir el mensaje de cabecera
    li $v0, 4               # syscall 4: print string
    la $a0, msg_print
    syscall

    # Comprobar si el buffer está vacío ($s0 == $s1)
    beq $s0, $s1, reset_cycle

print_buffer_loop:
    # Leer el carácter desde la posición de la cola ($s1)
    la $t4, buffer
    add $t4, $t4, $s1
    lb $a0, 0($t4)

    # Imprimir el carácter en la consola
    li $v0, 11              # syscall 11: print character
    syscall

    # Avanzar la cola de manera circular
    addi $s1, $s1, 1
    lw $t5, buffer_size
    rem $s1, $s1, $t5

    # Continuar hasta vaciar todo el buffer almacenado hasta head
    bne $s1, $s0, print_buffer_loop

reset_cycle:
    # Imprimir salto de línea final
    li $v0, 4
    la $a0, msg_nl
    syscall

    # Repetir el proceso indefinidamente
    j main_loop