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
