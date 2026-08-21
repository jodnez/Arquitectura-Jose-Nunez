.data
array: .word 38, 27, 43, 3, 9, 82, 10
array_size: .word 7
# Arreglo temporal auxiliar para el proceso de merge (debe tener espacio suficiente)
temp_array: .space 32
space_str: .asciiz " "
msg_init: .asciiz "Arreglo ordenado con mergesort: \n"

.text
.globl main

main:
	 #Cargar parametros iniciales
	 la $a0, array        # $a0 = direccion base del arreglo
	 li $a1, 0            # $a1 = left (indice 0)
	 lw $t0, array_size
	 addi $a2, $t0, -1    # $a2 = right (tamaño - 1)
	 
	 # Llamar a mergesort(array, 0, size - 1)
	 jal mergesort
	 
	 # Rutina para imprimir el arreglo ordenado
	 li $v0, 4
	 la $a0, msg_init
	 syscall
	 
	 la $t0, array        # $t0 = direccion base del arreglo
	 lw $t1, array_size   # $t1 = tamaño del arreglo
	 li $t2, 0
	 
print_loop:
	bge $t2, $t1, end_program
	
	sll $t3, $t2, 2
	add $t4, $t0, $t3
	lw $a0, 0 ($t4)       # $a0 = elemento a imprimir
	
	li $v0, 1 
	syscall
	
	li $v0, 4
	la $a0, space_str
	syscall
	
	addi $t2, $t2, 1      # i++
	j print_loop
	
end_program:
	li $v0, 10
	syscall

#-----------------------------------------------------------------------------------------------------------------
# Funcion: mergesort(array, left, right)
#-----------------------------------------------------------------------------------------------------------------
mergesort:
	# Prologo del stack: guardamos $ra, $s0 (left), $s1 (right), $s2 (mid), $s3 (array)
	addi $sp, $sp, -24
	sw $ra, 20($sp)
	sw $s0, 16($sp)
	sw $s1, 12($sp)
	sw $s2, 8($sp)
	sw $s3, 4($sp)
	
	move $s3, $a0        # $s3 + direccion base del arreglo
	move $s0, $a1        # $s0 = left
	move $s1, $a2        # $s1 = right
	
	# Si el subarreglo tiene 0 o 1 elemento, ya está ordenado
	# Condicion base: si left >= right, retornar
	bge $s0, $s1, ms_end
	
	# Dividir el arreglo actual en dos mitades
	# Calcular mid = (left + right) / 2
	add $t0, $s0, $s1
	sra $s2, $t0, 1       # $s2 = mid
	
	# Llamada recursiva izquierda: mergesort(array, left, mid)
	move $a0, $s3
	move $a1, $s0
	move $a2, $s2
	jal mergesort
	
	# Llamada recursiva derecha: mergesort(array, mid + 1, right)
	move $a0, $s3
	addi $a1, $s2, 1
	move $a2, $s1 
	jal mergesort
	
	# Combinar ambas mitades: merge(array, left, mid, right)
	move $a0, $s3
	move $a1, $s0
	move $a2, $s2
	# El cuarto argumento (right) lo pasamos mediante el registro $a3
	move $a3, $s1
	jal merge
	
ms_end:
	lw $ra, 20($sp)
	lw $s0, 16($sp)
	lw $s1, 12($sp)
	lw $s2, 8($sp)
	lw $s3, 4($sp)
	addi $sp, $sp, 24
	jr $ra
	
#-----------------------------------------------------------------------------------------------------------------
# Funcion: merge(array, left, mid, right)
# Argumentos: $a0 = array, $a1 = left, $a2 = mid, $a3 = right
#-----------------------------------------------------------------------------------------------------------------
merge:
	# Prologo para el merge
	addi $sp, $sp, -32
	sw $ra, 28($sp)
	sw $s0, 24($sp)       # left
	sw $s1, 20($sp)       # mid
	sw $s2, 16($sp)       # right
	sw $s3, 12($sp)       # direccion base array
	sw $s4, 8($sp)        # i
	sw $s5, 4($sp)        # j
	
	move $s3, $a0
	move $s0, $a1
	move $s1, $a2
	move $s2, $a3
	
	move $t0, $s0         # i apunta al inicio de la mitad izquierda
	addi $t1, $s1, 1      # j apunta al inicio de la mitad derecha
	move $t2, $s0         # k indica dónde escribir en el arreglo temporal
	la $t8, temp_array
	
	# Comparar el siguiente elemento de cada mitad y copiar
	# El menor al arreglo temporal
merge_compare_loop:
	bgt $t0, $s1, copy_remaining_right
	bgt $t1, $s2, copy_remaining_left
	
	sll $t3, $t0, 2
	add $t3, $s3, $t3
	lw $t4, 0($t3)       # arr[i]
	 
	sll $t5, $t1, 2
	add $t5, $s3, $t5
	lw $t6, 0($t5)     # arr[j]
	  
	bgt $t4, $t6, copy_right_element
	
	# Tomar de la izquierda
	sll $t7, $t2, 2
	add $t7, $t8, $t7
	sw $t4, 0($t7)
	addi $t0, $t0, 1
	j next_temp_position
	
copy_right_element:
	sll $t7, $t2, 2
	add $t7, $t8, $t7
	sw $t6, 0($t7)
	addi $t1, $t1, 1
	
next_temp_position:
	addi $t2, $t2, 1
	j merge_compare_loop

copy_remaining_left:
	bgt $t0, $s1, mg_copy_right_rest_init
	sll $t3, $t0, 2
	add $t3, $s3, $t3
	lw $t4, 0($t3) 
	sll $t7, $t2, 2
	add $t7, $t8, $t7
	sw $t4, 0($t7)
	addi $t0, $t0, 1
	addi $t2, $t2, 1
	j copy_remaining_left

mg_copy_right_rest_init:
copy_remaining_right:
	bgt $t1, $s2, prepare_copy_back
	sll $t5, $t1, 2
	add $t5, $s3, $t5
	lw $t6, 0($t5)
	sll $t7, $t2, 2
	add $t7, $t8, $t7 
	sw $t6, 0($t7)
	addi $t1, $t1, 1
	addi $t2, $t2, 1
	j copy_remaining_right

	# El arreglo temporal contiene los elementos ordenados
	# Copiarlos nuevamente al arreglo original
prepare_copy_back:
	move $t0, $s0         # $t0 = i = left (indice en el arreglo original)
	move $t4, $s0         # $t4 = k = left (indice en el arreglo temporal)

copy_back_loop:
	bgt $t0, $s2, merge_end # si i > right, terminar copia
	
	# Cargar desde temp_array[k]
	sll $t7, $t4, 2
	add $t9, $t8, $t7     # $t9 = &temp_array[k]
	lw $v1, 0($t9)        # $v1 = temp_array[k]
	
	# Guardar en array[i]
	sll $t7, $t0, 2
	add $t3, $s3, $t7      # $t3 = &array[i]
	sw $v1, 0($t3)	       # array[i] = $v1
	
	addi $t0, $t0, 1      # i++
	addi $t4, $t4, 1      # k++
	j copy_back_loop
	
	# Restaurar los registros guardados y regresar al llamador
merge_end:
	lw $ra, 28($sp)
	lw $s0, 24($sp)
	lw $s1, 20($sp)
	lw $s2, 16($sp)
	lw $s3, 12($sp)
	lw $s4, 8($sp)
	lw $s5, 4($sp)
	addi $sp, $sp, 32
	jr $ra
	
	   
