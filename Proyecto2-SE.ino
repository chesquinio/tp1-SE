<<<<<<< HEAD
const int FILAS = 4;
const int COLUMNAS = 4;

// Pines del teclado
byte pinesFilas[FILAS] = {5, 4, 3, 2};
byte pinesColumnas[COLUMNAS] = {6, 7, 8, 12};

// Matriz del teclado
char teclas[FILAS][COLUMNAS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Pines BCD a CD4511
const int BCD_PINS[4] = {A0, A1, A2, A3};

const int DIGIT_PINS[4] = {13, A4, A5};

const char codigoSecreto[] = "123";
char codigoIngresado[4] = "";
int intentosFallidos = 0;
int indiceCodigo = 0;
bool puertaBloqueada = false;
bool accesoConcedido = false;

// Pines del LED RGB
const int ledR = 9;
const int ledG = 10;
const int ledY = 11;

const int rele = 1;

enum EstadoLED {
  NORMAL,
  ACCESO,
  ERROR,
  BLOQUEADO
};

EstadoLED estadoActualLED = NORMAL;

// Variables de control de tiempo
unsigned long tiempoActual = 0;
unsigned long tiempoAnteriorParpadeo = 0;
unsigned long tiempoInicioAcceso = 0;
unsigned long tiempoErrorStart = 0;
bool estadoParpadeo = false;

// Displays
int digitos[3] = {10, 10, 10}; // Inicializados en guion (-)
int digitoActivo = 0;          // Control de multiplexado

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < FILAS; i++) {
    pinMode(pinesFilas[i], OUTPUT);
    digitalWrite(pinesFilas[i], HIGH);
  }
  
  for (int i = 0; i < COLUMNAS; i++) {
    pinMode(pinesColumnas[i], INPUT_PULLUP);
  }
  
  for (int i = 0; i < 4; i++) {
    pinMode(BCD_PINS[i], OUTPUT);
    digitalWrite(BCD_PINS[i], LOW);
    pinMode(DIGIT_PINS[i], OUTPUT);
    digitalWrite(DIGIT_PINS[i], LOW);
  }

  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledY, OUTPUT);
  pinMode(rele, OUTPUT);
  
  digitalWrite(rele, LOW);

  setColorRGB(0, 0, 255);

  Serial.println("Sistema inicializado. Ingrese el código de 3 dígitos:");
}

void loop() {
  tiempoActual = millis();
  
  actualizarDisplays();
  actualizarLedRGB();
  
  if (accesoConcedido) {
    if (tiempoActual - tiempoInicioAcceso >= 5000) {
      accesoConcedido = false;
      digitalWrite(rele, LOW);
      estadoActualLED = NORMAL;
      Serial.println("Puerta cerrada.");
    }
  }
  
  if (estadoActualLED == ERROR && tiempoActual - tiempoErrorStart >= 2000) {
    estadoActualLED = NORMAL;
  }
  
  char tecla = leerTeclado();
  if (tecla) {
    procesarTecla(tecla);
  }
}

void procesarTecla(char tecla) {
  if (tecla == 'A') {
    if (!puertaBloqueada && indiceCodigo == 3) {
      if (verificarCodigo()) {
        Serial.println("\nAcceso concedido. Puerta abierta.");
        accesoConcedido = true;
        tiempoInicioAcceso = tiempoActual;
        digitalWrite(rele, HIGH);
        estadoActualLED = ACCESO;
        
        // Mostrar 0-0-0 en los displays
        for (int i = 0; i < 3; i++) digitos[i] = 0;
      } else {
        Serial.println("\nCódigo incorrecto. Intento fallido.");
        intentosFallidos++;
        Serial.print("Intentos fallidos: ");
        Serial.println(intentosFallidos);
        
        estadoActualLED = ERROR;
        tiempoErrorStart = tiempoActual;
        
        if (intentosFallidos >= 3) {
          puertaBloqueada = true;
          estadoActualLED = BLOQUEADO;
          Serial.println("Puerta bloqueada. Presione 'B' para desbloquear.");
        }

        // Mostrar guiones
        for (int i = 0; i < 3; i++) digitos[i] = 10;
      }
      indiceCodigo = 0;
      for (int i = 0; i < 3; i++) codigoIngresado[i] = '\0';
    }
  }
  else if (tecla == 'B') { // Desbloqueo
    if (puertaBloqueada) {
      puertaBloqueada = false;
      intentosFallidos = 0;
      Serial.println("Puerta desbloqueada. Ingrese el código:");
      estadoActualLED = NORMAL;
      reiniciarCodigo();
    }
  }
  else if (tecla == 'C') { // Cancelar
    if (!puertaBloqueada && indiceCodigo > 0) {
      reiniciarCodigo();
      Serial.println("Código cancelado. Ingrese nuevamente:");
    }
  }
  else if (!puertaBloqueada && tecla >= '0' && tecla <= '9' && indiceCodigo < 3) {
    // Desplazar los dígitos ingresados
    digitos[2] = digitos[1];
    digitos[1] = digitos[0];
    digitos[0] = tecla - '0';
    
    codigoIngresado[indiceCodigo] = tecla;
    indiceCodigo++;
    
    Serial.print(tecla);
  }
}

void reiniciarCodigo() {
  indiceCodigo = 0;
  for (int i = 0; i < 3; i++) {
    codigoIngresado[i] = '\0';
    digitos[i] = 10; // Guion
  }
}

void actualizarDisplays() {
  static unsigned long ultimoCambio = 0;
  const unsigned long tiempoMultiplexado = 5;

  if (tiempoActual - ultimoCambio >= tiempoMultiplexado) {
    ultimoCambio = tiempoActual;

    // Apagar todos los displays
    for (int i = 0; i < 4; i++) {
      digitalWrite(DIGIT_PINS[i], LOW);
    }

    // Avanzar al siguiente dígito
    digitoActivo++;
    if (digitoActivo >= 3) {
      digitoActivo = 0;
    }

    // Ahora actualizar el valor BCD del número que queremos mostrar
    int valor = digitos[digitoActivo];

    if (valor >= 0 && valor <= 9) {
      for (int i = 0; i < 4; i++) {
        digitalWrite(BCD_PINS[i], bitRead(valor, i));
      }
    } else {
      // Si es 10 (guion), mandamos 0 o algo custom (podrías agregar un patrón de guion real)
      for (int i = 0; i < 4; i++) {
        digitalWrite(BCD_PINS[i], LOW);
      }
    }

    // Encender sólo el display actual
    digitalWrite(DIGIT_PINS[digitoActivo], HIGH);
  }
}



char leerTeclado() {
  static unsigned long ultimaTeclaPulsada = 0;
  const unsigned long tiempoDebounce = 200;
  
  if (tiempoActual - ultimaTeclaPulsada < tiempoDebounce) {
    return 0;
  }
  
  char tecla = 0;
  for (int fila = 0; fila < FILAS; fila++) {
    digitalWrite(pinesFilas[fila], LOW);
    for (int columna = 0; columna < COLUMNAS; columna++) {
      if (digitalRead(pinesColumnas[columna]) == LOW) {
        tecla = teclas[fila][columna];
        ultimaTeclaPulsada = tiempoActual;
        digitalWrite(pinesFilas[fila], HIGH);
        return tecla;
      }
    }
    digitalWrite(pinesFilas[fila], HIGH);
  }
  return 0;
}

bool verificarCodigo() {
  for (int i = 0; i < 3; i++) {
    if (codigoIngresado[i] != codigoSecreto[i]) {
      return false;
    }
  }
  return true;
}

void actualizarLedRGB() {
  switch (estadoActualLED) {
    case NORMAL:
      setColorRGB(255, 255, 0);
      break;
    case ACCESO:
      setColorRGB(0, 255, 0);
      break;
    case ERROR:
      setColorRGB(255, 0, 0);
      break;
    case BLOQUEADO:
      if (tiempoActual - tiempoAnteriorParpadeo >= 500) {
        tiempoAnteriorParpadeo = tiempoActual;
        estadoParpadeo = !estadoParpadeo;
        if (estadoParpadeo) setColorRGB(255, 255, 0);
        else setColorRGB(0, 0, 0);
      }
      break;
  }
}

void setColorRGB(int red, int green, int blue) {
  digitalWrite(ledR, red);
  digitalWrite(ledG, green);
  digitalWrite(ledY, blue);
}
=======
const int FILAS = 4;      // Número de filas del teclado
const int COLUMNAS = 4;   // Número de columnas del teclado

// Pines conectados a las filas del teclado
byte pinesFilas[FILAS] = {5, 4, 3, 2};

// Pines conectados a las columnas del teclado
byte pinesColumnas[COLUMNAS] = {6, 7, 8, 12};

// Matriz que representa las teclas del teclado
char teclas[FILAS][COLUMNAS] = {
  {'1', '2', '3', 'A'},  // A = Enter
  {'4', '5', '6', 'B'},  // B = Desbloqueo
  {'7', '8', '9', 'C'},  // C = Cancel
  {'*', '0', '#', 'D'}   // No se usa
};

const int BCD_PINS[4] = {A0, A1, A2, A3};  // A = A0, B = A1, C = A2, D = A3
const int LATCH_PIN = 10;                 // LE del 4511
const int ENABLE_PIN = 11;   

const char codigoSecreto[] = "123";  // Código secreto de 3 dígitos
char codigoIngresado[4] = "";        // Almacena el código ingresado por el usuario
int intentosFallidos = 0;
int indiceCodigo = 0;
bool puertaBloqueada = false;
bool accesoConcedido = false;

// Pines para el LED RGB
const int ledR = 9;          // Pin rojo del LED RGB (RX) - Desconectar durante programación
const int ledG = 10;          // Pin verde del LED RGB (TX) - Desconectar durante programación
const int ledY = 11;         // Pin azul del LED RGB (si está disponible, sino usar otro)

// LED de funcionamiento y relé
const int ledFuncionamiento = 17;  // Otro pin disponible (puede ser virtual en placas extendidas)
const int pinRele = A7;            // Pin del relé

// Enumeración para los estados del LED RGB
enum EstadoLED {
  NORMAL,      // Estado normal (amarillo fijo)
  ACCESO,      // Acceso concedido (verde)
  ERROR,       // Error en clave (rojo)
  BLOQUEADO    // Bloqueado (amarillo intermitente)
};

EstadoLED estadoActualLED = NORMAL;

// Variables para control de tiempo sin usar delay()
unsigned long tiempoActual = 0;
unsigned long tiempoAnteriorParpadeo = 0;
unsigned long tiempoInicioAcceso = 0;
unsigned long tiempoRefrescoDisplay = 0;
unsigned long tiempoErrorStart = 0;
bool estadoParpadeo = false;

// Guarda los dígitos ingresados (0-9) o -1 para guión o apagado
int digitos[3] = {10, 10, 10};  // Inicializado con guiones
int digitoActivo = 0;           // Display actualmente activo

void setup() {
  Serial.begin(9600);
  
  // Configurar los pines de las filas como salidas
  for (int i = 0; i < FILAS; i++) {
    pinMode(pinesFilas[i], OUTPUT);
    digitalWrite(pinesFilas[i], HIGH);  // Inicializar filas en alto
  }
  
  // Configurar los pines de las columnas como entradas con resistencias pull-up internas
  for (int i = 0; i < COLUMNAS; i++) {
    pinMode(pinesColumnas[i], INPUT_PULLUP);
  }

  for (int i = 0; i < 4; i++) {
    pinMode(BCD_PINS[i], OUTPUT);
    digitalWrite(BCD_PINS[i], LOW);
  }
  pinMode(LATCH_PIN, OUTPUT);
  digitalWrite(LATCH_PIN, LOW);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW); 
  
  // Configurar LEDs y relé
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledY, OUTPUT);
  pinMode(ledFuncionamiento, OUTPUT);
  pinMode(pinRele, OUTPUT);
  
  // Inicializar LEDs y relé
  digitalWrite(ledFuncionamiento, HIGH);  // LED de funcionamiento siempre encendido
  digitalWrite(pinRele, LOW);             // Relé desactivado
  
  // Establecer color inicial del LED RGB (amarillo: R+G)
  setColorRGB(0, 0, 255);  // Amarillo = Rojo + Verde
  
  Serial.println("Sistema inicializado. Ingrese el código de 3 dígitos:");
}

void loop() {
  tiempoActual = millis();
  
  // Actualizar los displays (multiplexación)
  actualizarDisplays();
  
  // Actualizar estado del LED RGB
  actualizarLedRGB();
  
  // Controlar el acceso concedido (relé activado por 5 segundos)
  if (accesoConcedido) {
    if (tiempoActual - tiempoInicioAcceso >= 5000) {
      accesoConcedido = false;
      digitalWrite(pinRele, LOW);         // Desactivar relé
      estadoActualLED = NORMAL;           // Volver al estado normal
      Serial.println("Puerta cerrada.");
    }
  }
  
  // Si estado error por más de 2 segundos, volver a normal
  if (estadoActualLED == ERROR && tiempoActual - tiempoErrorStart >= 2000) {
    estadoActualLED = NORMAL;
  }
  
  // Leer teclado sin delay
  char teclaPulsada = leerTeclado();
  if (teclaPulsada) {
    // Procesar tecla Enter (A)
    if (teclaPulsada == 'A') {
      if (!puertaBloqueada && indiceCodigo == 3) {
        if (verificarCodigo()) {
          Serial.println("\nAcceso concedido. Puerta abierta.");
          accesoConcedido = true;
          tiempoInicioAcceso = tiempoActual;
          digitalWrite(pinRele, HIGH);      // Activar relé
          estadoActualLED = ACCESO;         // LED verde
        } else {
          Serial.println("\nCódigo incorrecto. Intento fallido.");
          intentosFallidos++;
          Serial.print("Intentos fallidos: ");
          Serial.println(intentosFallidos);
          
          estadoActualLED = ERROR;          // LED rojo
          tiempoErrorStart = tiempoActual;
          
          if (intentosFallidos >= 3) {
            puertaBloqueada = true;
            estadoActualLED = BLOQUEADO;    // LED amarillo intermitente
            Serial.println("Puerta bloqueada. Presione 'B' para desbloquear.");
          }
        }
        // Reiniciar código y mostrar guiones en los displays
        reiniciarCodigo();
      } else if (indiceCodigo > 0 && indiceCodigo < 3) {
        Serial.println("\nDebe ingresar 3 dígitos.");
      }
    }
    // Procesar tecla Desbloqueo (B)
    else if (teclaPulsada == 'B') {
      if (puertaBloqueada) {
        puertaBloqueada = false;
        intentosFallidos = 0;
        Serial.println("Puerta desbloqueada. Ingrese el código:");
        estadoActualLED = NORMAL;           // Volver a LED amarillo normal
        reiniciarCodigo();
      }
    }
    // Procesar tecla Cancel (C)
    else if (teclaPulsada == 'C') {
      if (!puertaBloqueada && indiceCodigo > 0) {
        reiniciarCodigo();
        Serial.println("Código cancelado. Ingrese nuevamente:");
      }
    }
    // Procesar dígitos
    else if (!puertaBloqueada && teclaPulsada >= '0' && teclaPulsada <= '9' && indiceCodigo < 3) {
      codigoIngresado[indiceCodigo] = teclaPulsada;
      digitos[indiceCodigo] = teclaPulsada - '0';  // Convertir char a int
      Serial.print(teclaPulsada);
      indiceCodigo++;
    }
  }
}

char leerTeclado() {
  static unsigned long ultimaTeclaPulsada = 0;
  const unsigned long tiempoDebounce = 200;  // 200ms debounce
  
  if (tiempoActual - ultimaTeclaPulsada < tiempoDebounce) {
    return 0;  // No ha pasado suficiente tiempo desde la última pulsación
  }
  
  char tecla = 0;
  
  for (int fila = 0; fila < FILAS; fila++) {
    digitalWrite(pinesFilas[fila], LOW);
    for (int columna = 0; columna < COLUMNAS; columna++) {
      if (digitalRead(pinesColumnas[columna]) == LOW) {
        tecla = teclas[fila][columna];
        ultimaTeclaPulsada = tiempoActual;
        digitalWrite(pinesFilas[fila], HIGH);
        return tecla;
      }
    }
    digitalWrite(pinesFilas[fila], HIGH);
  }
  
  return 0;  // No se presionó ninguna tecla
}

bool verificarCodigo() {
  for (int i = 0; i < 3; i++) {
    if (codigoIngresado[i] != codigoSecreto[i]) {
      return false;
    }
  }
  return true;
}

void reiniciarCodigo() {
  indiceCodigo = 0;
  for (int i = 0; i < 3; i++) {
    codigoIngresado[i] = '\0';
    digitos[i] = 10;  // Mostrar guiones
  }
}

void actualizarDisplays() {
  static int ultimoDigitoMostrado = -1;
  int valor = digitos[digitoActivo];

  if (valor >= 0 && valor <= 9 && valor != ultimoDigitoMostrado) {
    for (int i = 0; i < 4; i++) {
      digitalWrite(BCD_PINS[i], bitRead(valor, i));
    }
    digitalWrite(LATCH_PIN, HIGH);
    delayMicroseconds(1); // breve pulso de latch
    digitalWrite(LATCH_PIN, LOW);
    ultimoDigitoMostrado = valor;
  }
}

void actualizarLedRGB() {
  switch (estadoActualLED) {
    case NORMAL:
      // Amarillo fijo (R+G)
      setColorRGB(255, 255, 0);
      break;
      
    case ACCESO:
      // Verde
      setColorRGB(0, 255, 0);
      break;
      
    case ERROR:
      // Rojo
      setColorRGB(255, 0, 0);
      break;
      
    case BLOQUEADO:
      // Amarillo intermitente
      if (tiempoActual - tiempoAnteriorParpadeo >= 500) {
        tiempoAnteriorParpadeo = tiempoActual;
        estadoParpadeo = !estadoParpadeo;
        
        if (estadoParpadeo) {
          setColorRGB(255, 255, 0);  // Amarillo
        } else {
          setColorRGB(0, 0, 0);      // Apagado
        }
      }
      break;
  }
}

void setColorRGB(int red, int green, int blue) {
  // Para LED RGB con ánodo común (invertir salidas)
  // Si el LED es de cátodo común, quitar la negación (!)
  digitalWrite(ledR, red);
  digitalWrite(ledG, green);
  digitalWrite(ledY, blue);
}
>>>>>>> 5bc1705e6547b2b13bea02557130c5a28506cc54
