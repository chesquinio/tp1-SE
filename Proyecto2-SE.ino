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

// Pines para los dígitos
const int DIGIT_PINS[3] = {13, A4, A5};

const char codigoSecreto[] = "123";
char codigoIngresado[4] = "";
int intentosFallidos = 0;
int indiceCodigo = 0;
bool puertaBloqueada = false;
bool accesoConcedido = false;

// Pines del LED RGB
const int ledR = 9;
const int ledG = 10;
const int ledY = 14;

const int rele = 11;

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
int digitos[3] = {10, 10, 10}; // Guiones
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
  }

  for (int i = 0; i < 3; i++) {
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
  
  if (accesoConcedido && (tiempoActual - tiempoInicioAcceso >= 5000)) {
    accesoConcedido = false;
    digitalWrite(rele, LOW);
    estadoActualLED = NORMAL;
    Serial.println("Puerta cerrada.");
  }
  
  if (estadoActualLED == ERROR && (tiempoActual - tiempoErrorStart >= 2000)) {
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
        for (int i = 0; i < 3; i++) digitos[i] = 10;
      }
      reiniciarCodigo();
    }
  } else if (tecla == 'B') { // Desbloquear
    if (puertaBloqueada) {
      puertaBloqueada = false;
      intentosFallidos = 0;
      Serial.println("Puerta desbloqueada. Ingrese el código:");
      estadoActualLED = NORMAL;
      reiniciarCodigo();
    }
  } else if (tecla == 'C') { // Cancelar
    if (!puertaBloqueada && indiceCodigo > 0) {
      reiniciarCodigo();
      Serial.println("Código cancelado. Ingrese nuevamente:");
    }
  } else if (!puertaBloqueada && tecla >= '0' && tecla <= '9' && indiceCodigo < 3) {
    // Almacenar el nuevo dígito en el array codigoIngresado
    codigoIngresado[indiceCodigo] = tecla;
    indiceCodigo++;
    
    // Actualizar los valores para mostrar en los displays
    // Display 2 (izquierda) muestra el primer dígito ingresado cuando hay 3 dígitos
    // Display 1 (centro) muestra el primer dígito cuando hay 2, o el segundo cuando hay 3
    // Display 0 (derecha) siempre muestra el último dígito ingresado
    
    // Limpiar primero todos los dígitos
    for (int i = 0; i < 3; i++) {
      digitos[i] = 10; // Valor para guion
    }
    
    // Llenar los displays con los valores correspondientes
    for (int i = 0; i < indiceCodigo; i++) {
      // Calculamos la posición en el array digitos (de derecha a izquierda)
      int posDisplay = indiceCodigo - 1 - i;
      // Asignamos el valor del dígito correspondiente
      digitos[posDisplay] = codigoIngresado[i] - '0';
    }
    
    Serial.print(tecla);
  }
}

void reiniciarCodigo() {
  indiceCodigo = 0;
  for (int i = 0; i < 3; i++) {
    codigoIngresado[i] = '\0';
    digitos[i] = 10; // Guión
  }
}

void actualizarDisplays() {
  static unsigned long ultimoCambio = 0;
  const unsigned long intervaloMultiplexado = 5; // 5 ms por dígito

  if (tiempoActual - ultimoCambio >= intervaloMultiplexado) {
    ultimoCambio = tiempoActual;

    // Apagar todos los dígitos
    for (int i = 0; i < 3; i++) {
      digitalWrite(DIGIT_PINS[i], LOW);
    }

    // Avanzar al siguiente dígito
    digitoActivo = (digitoActivo + 1) % 3;

    int valor = digitos[digitoActivo];

    // Limpiar pines BCD antes de establecer el nuevo valor
    for (int i = 0; i < 4; i++) {
      digitalWrite(BCD_PINS[i], LOW);
    }

    // Setear BCD
    if (valor >= 0 && valor <= 9) {
      // Valor numérico (del 0 al 9)
      for (int i = 0; i < 4; i++) {
        digitalWrite(BCD_PINS[i], bitRead(valor, i));
      }
    } else {
      // Si es guion (valor 10), ajustamos los pines BCD manualmente
      // Para el CD4511 podemos usar el patrón 1011 (valor 11 decimal)
      // que en muchos displays muestra como guión
      digitalWrite(BCD_PINS[0], HIGH); // Bit 0 = 1
      digitalWrite(BCD_PINS[1], HIGH); // Bit 1 = 1
      digitalWrite(BCD_PINS[2], LOW);  // Bit 2 = 0
      digitalWrite(BCD_PINS[3], HIGH); // Bit 3 = 1
    }

    // Encender solo el dígito actual
    digitalWrite(DIGIT_PINS[digitoActivo], HIGH);
  }
}

char leerTeclado() {
  static unsigned long ultimaTecla = 0;
  const unsigned long debounceDelay = 200;

  if (tiempoActual - ultimaTecla < debounceDelay) return 0;
  
  for (int fila = 0; fila < FILAS; fila++) {
    digitalWrite(pinesFilas[fila], LOW);
    for (int col = 0; col < COLUMNAS; col++) {
      if (digitalRead(pinesColumnas[col]) == LOW) {
        ultimaTecla = tiempoActual;
        digitalWrite(pinesFilas[fila], HIGH);
        return teclas[fila][col];
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
        setColorRGB(estadoParpadeo ? 255 : 0, estadoParpadeo ? 255 : 0, 0);
      }
      break;
  }
}

void setColorRGB(int red, int green, int blue) {
  digitalWrite(ledR, red > 0 ? HIGH : LOW);
  digitalWrite(ledG, green > 0 ? HIGH : LOW);
  digitalWrite(ledY, blue > 0 ? HIGH : LOW);
}
