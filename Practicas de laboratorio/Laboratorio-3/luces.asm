.data
LuzControl:    .word 0
LuzEstado:     .word 0
LuzDatos:      .word 0
msgError:      .asciiz "Error de hardware detectado en el sensor (-1).\n"
msgExito:      .asciiz "Sensor inicializado y listo. Lectura obtenida: "
msgSalto:      .asciiz "\n"

.text
.globl main

main:
    # 1. Llamamos al procedimiento de inicialización
    jal InicializarSensorLuz
    
    # Verificamos si la inicialización terminó en error (-1)
    la   $t0, LuzEstado
    lw   $t1, 0($t0)
    li   $t2, -1
    beq  $t1, $t2, ImprimirError

    # 2. Si todo salió bien, llamamos a LeerLuminosidad
    jal LeerLuminosidad
    
    # Verificamos si la lectura fue exitosa ($v0 == 0)
    bnez $v0, ImprimirError
    
    # Imprimimos mensaje de éxito
    li   $v0, 4
    la   $a0, msgExito
    syscall
    
    # Imprimimos el valor numérico de la luminosidad ($v1)
    li   $v0, 1
    move $a1, $v1       # Guardamos temporalmente el valor
    move $a0, $v1
    syscall
    
    # Salto de línea final
    li   $v0, 4
    la   $a0, msgSalto
    syscall
    
    j    Fin

ImprimirError:
    li   $v0, 4              # Syscall para imprimir cadena
    la   $a0, msgError
    syscall

Fin:
    # Fin del programa limpio
    li   $v0, 10
    syscall

# ============================================================================
# Procedimiento: InicializarSensorLuz
# ============================================================================
InicializarSensorLuz:
    la   $t0, LuzControl
    la   $t1, LuzEstado
    
    li   $t2, 1
    sw   $t2, 0($t0)

LoopEsperaListo:
    lw   $t3, 0($t1)
    li   $t4, -1
    beq  $t3, $t4, ErrorHardware
    
    li   $t4, 1
    beq  $t3, $t4, SensorListo
    
    j    LoopEsperaListo

ErrorHardware:
    jr   $ra

SensorListo:
    jr   $ra

# ============================================================================
# Procedimiento: LeerLuminosidad
# ============================================================================
LeerLuminosidad:
    la   $t0, LuzEstado
    la   $t1, LuzDatos
    
    lw   $t2, 0($t0)
    li   $t3, 1
    beq  $t2, $t3, LecturaCorrecta
    
    li   $v0, 1         # Código de error
    li   $v1, 0         
    jr   $ra

LecturaCorrecta:
    lw   $v1, 0($t1)    # Lee el dato de luminosidad
    li   $v0, 0         # Código de éxito (0)
    jr   $ra