# Chat con Colas de Mensajes (IPC)

Este proyecto implementa un sistema de chat multiusuario basado en **colas de mensajes de System V** en C.  
Permite a múltiples clientes conectarse a un servidor central, crear salas, enviar mensajes y listar usuarios.

---

## 📦 Requisitos

- Sistema operativo **Linux** 
- Compilador **gcc**.  
- Biblioteca **pthread** (para el cliente).

---

### 1. Clonar el repositorio

```bash
git clone https://github.com/SSloan07/Parcial2.git
cd Parcial2
```

### 2. Compilar

```bash
gcc -o cliente.out cliente.c -lpthread
gcc -o servidor.out servidor.c -lpthread
```
### 3. Ejecutar

En una terminal, inicia el servidor:

```bash
./servidor.out
```

En tantas terminales como quieras, abre clientes diferentes.
Cada cliente requiere un nombre de usuario:

```bash
./cliente.out <NombreUsuario>
```


