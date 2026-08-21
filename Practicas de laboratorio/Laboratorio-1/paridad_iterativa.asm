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
	move $t0, $v0 # $t0 = 0
	
	# Validacion: si n < 0, terminar
	bltz $t0, exit
	
	# Logica iterativa
	li $v0, 0 # %v0 = resultado inicial
	li $t1, 1 # Constante 1

bucle:
	blez $t0, fin # Si n <= 0, salir
	sub $v0, $t1, $v0 # $v0 = 1 - $v0
	sub $t0, $t0, 1 # n = n - 1
	j bucle
	
fin: 
	# Imprimir resultado
	move $t2, $v0
	li $v0, 4
	la $a0, result_msg
	syscall
	
	move $a0, $t2
	li $v0, 1
	syscall

exit:
	li $v0, 10
	syscall