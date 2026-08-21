.data
# Ejemplo de arreglo a ordenar (se puede cambiar o ampliar
array: .word 38, 27, 43, 3, 9, 82, 10
array_size: .word 7
space_str: .asciiz " "
msg_init: .asciiz "Arreglo ordenado: \n"

.text
.globl main

main:
	# Cargar direccion base del arreglo y tamaño
	la $a0, array         # $a0 = direccion base del arreglo
	li $a1, 0             # $a1 = low (indice 0)
	lw $t0, array_size
	addi $a2, $t0, -1     # $a2 = high (tamaño - 1)
	
	# Llamar a quicksort(array, 0, size - 1)
	jal quicksort
	
	#--- Rutina para imprimir el arreglo ordenado ---
	# Imprimir mensaje  inicial
	li $v0, 4
	la $a0, msg_init
	syscall
	
	# Configurar bucle de impresion
	la $t0, array         # $t0 = direccion base del arreglo
	lw $t1, array_size    # $t1 = contador (tamaño del arreglo)
	li $t2, 0             # $t2 = indice actual (i = 0)
	
print_loop:
	bge $t2, $t1, end_program
	
	# Cargar elemento actual del arreglo
	sll $t3, $t2, 2
	add $t4, $t0, $t3
	lw $a0, 0($t4)	      # $a0 = elemento a imprimir
	
	# Imprimir entero
	li $v0, 1
	syscall
	
	# Imprimir un espacio entre numeros
	li $v0, 4
	la $a0, space_str
	syscall
	
	addi $t2, $t2, 1     # i++
	j print_loop
	  
end_program:
	# Fin del programa (exit syscall)
	li $v0, 10
	syscall
	
#-----------------------------------------------------------------------------------------------------------------
# Funcion: quicksort(array, low, high)
# Argumentos: 
#    $a0 = direccion base del arreglo
#    $a1 = indice low
#    $a2 = indice high (lo manejaremos guardandolo en registros o pila)
# Nota: Para simplificar registros en recursion, guardaremos low y high en $s0 y $s1
#-----------------------------------------------------------------------------------------------------------------
quicksort:
	# Prologo del stack
	addi $sp, $sp, -24
	sw $ra, 20($sp)
	sw $s0, 16($sp)       # low
	sw $s1, 12($sp)       # high
	sw $s2, 8($sp)        # pi (indice de particion)
	sw $s3, 4($sp)        # direccion base del arreglo
	
	move $s3, $a0         # $s3 = direccion base del arreglo
	move $s0, $a1         # $s0 = low
	move $s1, $a2         # $s1 = high
	
	# Condicion base: si (low >= high) salir
	bge $s0, $s1, qs_end
	
	# Llamar a partition(array, low, high)
	move $a0, $s3
	move $a1, $s0
	move $a2, $s1
	jal partition
	move $s2, $v0         # $s2 = pi (posicion del pivote)
	
	# Llamar recursivamente a quicksort (array, low, pi - 1)
	move $a0, $s3         # Restaurar direccion base por si acaso
	move $a1, $s0
	addi $a2, $s2, -1
	jal quicksort
	
	# Llamar recursivamente a quicksort(array, pi + 1, high)
	move $a0, $s3
	addi $a1, $s2, 1
	move $a2, $s1
	jal quicksort
	
qs_end:
	# Epilogo del stack
	lw $ra, 20($sp)
	lw $s0, 16($sp)
	lw $s1, 12($sp)
	lw $s2, 8($sp)
	lw $s3, 4($sp)
	addi $sp, $sp, 24
	jr $ra
	
#-----------------------------------------------------------------------------------------------------------------
# Funcion: partition(array, low, high)
# Implementacion estandar del esquema de particion de lomuto
#-----------------------------------------------------------------------------------------------------------------
partition:
	# $a0 = array, $a1 = low, $a2 = high
	# Obtener el elemento pivote (arr[high])
	sll $t0, $a2, 2       # $t0 = high * 4 (offset)
	add $t1, $a0, $t0     # $t1 = &arr[high]
	lw $v1, 0($t1)        # $v1 = pivot = arr[high]

	addi $t2, $a1, -1     # $t2 = i = low - 1 (indice del elemento menor)
	move $t3, $a1         # $t3 = j = low
	
partition_loop:
	bge $t3, $a2, partition_end  # si j >= high, terminar bucle
	
	# Cargar arr[j]
	sll $t4, $t3, 2
	add $t5, $a0, $t4
	lw $t6, 0($t5)               # $t6 = arr[j]
	
	# si arr[j] > pivot, saltar incremento de i
	bgt $t6, $v1, partition_continue
	
	# i++
	addi $t2, $t2, 1
	
	# Intercambiar arr[i] y arr[j]
	sll $t7, $t2, 2
	add $t8, $a0, $t7
	lw $t9, 0($t8)        # $t9 = arr[i]
	sw $t6, 0($t8)        # arr[i] = arr[j]
	sw $t9, 0($t5)        # arr[j] = arr[t9]
	
partition_continue:
	addi $t3, $t3, 1      # j++
	j partition_loop
	
partition_end:
	# Intecambiar arr[i + 1] con el pivote arr[high]
	addi $t0, $t2, 1  # i + 1
	sll $t4, $t0, 2
	add $t5, $a0, $t4
	lw $t6, 0($t5)           # arr[i+1]
	
	sll $t7, $a2, 2
	add $t8, $a0, $t7
	sw $t6, 0($t8)           # arr[high] = arr[i+1]
	sw $v1, 0($t5)           # arr[i+1] = pivot
	
	# Retornar 1 + 1
	addi $v0, $t2, 1
	jr $ra