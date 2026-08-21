.data
prompt: .asciiz "Ingrese un numero entero positivo: "
result_msg: .asciiz "La paridad es: "

.text
.globl main

main:
	# Solicitar numero
	li $v0, 4
	la $a0, prompt
	syscall
	
	# Leer el numero
	li $v0, 5
	syscall
	move $a0, $v0
	
	# Validacion: si n < 0, terminar
	bltz $a0, exit
	
	jal paridad
	
	# Imprimir el resultado
	move $t0, $v0
	li $v0, 4
	la $a0, result_msg
	syscall
	
	move $a0, $t0
	li $v0,1
	syscall
	
exit:
	li $v0, 10
	syscall
	
# Funcion paridad(n)
paridad:
	# Preparar stack frame
	subu $sp, $sp, 8
	sw $ra, 4($sp)
	sw $a0, 0($sp)
	
	# Caso base: si n == 0, paridad(0) = 0
	bgtz $a0, recursivo
	li $v0, 0 
	j fin_paridad

recursivo:
	# Paridad(n-1)
	sub $a0, $a0, 1
	jal paridad
	
	# Calcular: 1 - paidad(n-1)
	li $t0, 1
	sub $v0, $t0, $v0 

fin_paridad:
	# Restaurar contexto	
	lw $ra, 4($sp)
	lw $a0, 0($sp)
	addu $sp, $sp, 8
	jr $ra	