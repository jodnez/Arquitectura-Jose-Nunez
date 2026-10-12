.data
TensionControl: .word 0
TensionEstado:  .word 0
TensionSistol:  .word 0
TensionDiastol: .word 0

msgExito:       .asciiz "Medicion de tension completada.\nPresion Sistolica: "
msgGuion:       .asciiz " / Diastolica: "
msgSalto:       .asciiz "\n"

.text
.globl main

main:
    # Llamamos al controlador de tensión
    jal controlador_tension
    
    # Al retornar, $v0 tiene la sistólica y $v1 la diastólica
    # Guardamos temporalmente los valores en registros $s
    move $s0, $v0
    move $s1, $v1

    # Imprimir mensaje de éxito y presión sistólica
    li   $v0, 4
    la   $a0, msgExito
    syscall

    li   $v0, 1
    move $a0, $s0
    syscall

    # Imprimir separador y presión diastólica
    li   $v0, 4
    la   $a0, msgGuion
    syscall

    li   $v0, 1
    move $a0, $s1
    syscall

    # Salto de línea final
    li   $v0, 4
    la   $a0, msgSalto
    syscall

    # Fin limpio del programa
    li   $v0, 10
    syscall

# ============================================================================
# Procedimiento: controlador_tension
# ============================================================================
controlador_tension:
    # 1. Iniciar la medición escribiendo el valor 1 en TensionControl
    la   $t0, TensionControl
    li   $t1, 1
    sw   $t1, 0($t0)

    # Cargar la dirección de TensionEstado para el bucle de espera
    la   $t2, TensionEstado

LoopEsperaTension:
    # 2. Esperar mientras TensionEstado sea 0[cite: 1]
    lw   $t3, 0($t2)
    li   $t4, 1
    bne  $t3, $t4, LoopEsperaTension   

    # 3. Cuando TensionEstado es 1, los resultados están listos[cite: 1]
    la   $t5, TensionSistol
    la   $t6, TensionDiastol
    
    lw   $v0, 0($t5)    # Retorna tensión sistólica en $v0[cite: 1]
    lw   $v1, 0($t6)    # Retorna tensión diastólica en $v1[cite: 1]
    
    jr   $ra