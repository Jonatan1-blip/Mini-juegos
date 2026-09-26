#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#endif

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>

    int _kbhit() {
        struct termios oldt, newt;
        int ch, oldf;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);
        if (ch != EOF) {
            ungetc(ch, stdin);
            return 1;
        }
        return 0;
    }

    int _getch() {
        struct termios oldt, newt;
        int ch;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
    }
#endif

using namespace std;

#define RESET             "\033[0m"
#define CLARA_COLOR       "\033[1;36m"
#define DAMA_CLARA_COLOR  "\033[1;32m"
#define OSCURA_COLOR      "\033[1;31m"
#define DAMA_OSCURA_COLOR "\033[1;33m"
#define GRID_COLOR        "\033[90m"
#define CURSOR_COLOR      "\033[1;47;30m"
#define SELECCION_COLOR   "\033[1;44;37m"

enum TipoFicha { VACIO, CLARA, OSCURA, DAMA_CLARA, DAMA_OSCURA };

const int TIEMPO_TURNO_SEGUNDOS = 15;

void activarPantallaModoJuego() {
    
    cout << "\033[?1049h\033[?25l" << flush;
}

void desactivarPantallaModoJuego() {

    cout << "\033[?25h\033[?1049l" << flush;
}

void limpiarYRedibujar() {

    cout << "\033[1;1H\033[2J";
}

class TableroDamas {
private:
    vector<vector<TipoFicha>> tablero;
    bool turnoClaras;
    int cursorFila, cursorCol;
    int selFila, selCol;
    vector<string> historialMovimientos;
    string mensajeEstado;

public:
    TableroDamas() {
        tablero = vector<vector<TipoFicha>>(8, vector<TipoFicha>(8, VACIO));
        inicializarTablero();
        turnoClaras = true;
        cursorFila = 5;
        cursorCol = 0;
        selFila = -1;
        selCol = -1;
        mensajeEstado = "Usa W,A,S,D para moverte y ESPACIO/ENTER para seleccionar.";
    }

    void inicializarTablero() {
        for (int f = 0; f < 3; ++f) {
            for (int c = 0; c < 8; ++c) {
                if ((f + c) % 2 != 0) tablero[f][c] = OSCURA;
            }
        }
        for (int f = 5; f < 8; ++f) {
            for (int c = 0; c < 8; ++c) {
                if ((f + c) % 2 != 0) tablero[f][c] = CLARA;
            }
        }
    }

    bool esTurnoValido(TipoFicha ficha) {
        if (turnoClaras && (ficha == CLARA || ficha == DAMA_CLARA)) return true;
        if (!turnoClaras && (ficha == OSCURA || ficha == DAMA_OSCURA)) return true;
        return false;
    }

    bool esEnemigo(TipoFicha propia, TipoFicha objetivo) {
        if (objetivo == VACIO) return false;
        bool esClara = (propia == CLARA || propia == DAMA_CLARA);
        bool objetivoEsOscura = (objetivo == OSCURA || objetivo == DAMA_OSCURA);
        bool objetivoEsClara = (objetivo == CLARA || objetivo == DAMA_CLARA);
        return (esClara && objetivoEsOscura) || (!esClara && objetivoEsClara);
    }

    void cambiarTurno() {
        turnoClaras = !turnoClaras;
        selFila = -1;
        selCol = -1;
    }

    void forzarCambioPorTiempo() {
        mensajeEstado = string("[TIEMPO AGOTADO] Turno de ") + (turnoClaras ? "Las Claras" : "Las Oscuras") + " finalizado.";
        cambiarTurno();
    }

    void moverCursor(int c) {
        if (c == 'w' || c == 'W') { if (cursorFila > 0) cursorFila--; }
        else if (c == 's' || c == 'S') { if (cursorFila < 7) cursorFila++; }
        else if (c == 'a' || c == 'A') { if (cursorCol > 0) cursorCol--; }
        else if (c == 'd' || c == 'D') { if (cursorCol < 7) cursorCol++; }
    }

    bool procesarAccion() {
        if (selFila == -1 && selCol == -1) {
            TipoFicha ficha = tablero[cursorFila][cursorCol];
            if (ficha == VACIO) {
                mensajeEstado = "[Error] Casilla vacia.";
                return false;
            }
            if (!esTurnoValido(ficha)) {
                mensajeEstado = "[Error] No es tu turno o la ficha no te pertenece.";
                return false;
            }
            selFila = cursorFila;
            selCol = cursorCol;
            mensajeEstado = "Ficha seleccionada. Mueve al destino y presiona ESPACIO/ENTER.";
            return false;
        } 
        else if (selFila == cursorFila && selCol == cursorCol) {
            selFila = -1;
            selCol = -1;
            mensajeEstado = "Seleccion cancelada.";
            return false;
        }
        else {
            int destFila = cursorFila;
            int destCol = cursorCol;

            if (tablero[destFila][destCol] != VACIO) {
                mensajeEstado = "[Error] La casilla de destino debe estar vacia.";
                return false;
            }

            int df = destFila - selFila;
            int dc = destCol - selCol;
            TipoFicha ficha = tablero[selFila][selCol];
            bool esDama = (ficha == DAMA_CLARA || ficha == DAMA_OSCURA);
            int dirValida = (ficha == CLARA || ficha == DAMA_CLARA) ? -1 : 1;

            if (abs(df) == 1 && abs(dc) == 1) {
                if (!esDama && (df != dirValida)) {
                    mensajeEstado = "[Error] Ficha normal no puede retroceder.";
                    return false;
                }

                tablero[destFila][destCol] = ficha;
                tablero[selFila][selCol] = VACIO;
                verificarCoronacion(destFila, destCol);

                string registro = (turnoClaras ? "Claras: (" : "Oscuras: (") + to_string(selFila) + "," + to_string(selCol) + ") -> (" + to_string(destFila) + "," + to_string(destCol) + ")";
                historialMovimientos.push_back(registro);

                mensajeEstado = "[Movimiento Exitoso]";
                cambiarTurno();
                return true;
            }
            else if (abs(df) == 2 && abs(dc) == 2) {
                if (!esDama && (df / 2 != dirValida)) {
                    mensajeEstado = "[Error] Ficha normal no puede retroceder.";
                    return false;
                }

                int medioFila = selFila + df / 2;
                int medioCol = selCol + dc / 2;
                TipoFicha enemiga = tablero[medioFila][medioCol];

                if (esEnemigo(ficha, enemiga)) {
                    tablero[destFila][destCol] = ficha;
                    tablero[selFila][selCol] = VACIO;
                    tablero[medioFila][medioCol] = VACIO;

                    verificarCoronacion(destFila, destCol);

                    string registro = (turnoClaras ? "Claras: CAPTURA (" : "Oscuras: CAPTURA (") + to_string(selFila) + "," + to_string(selCol) + ") -> (" + to_string(destFila) + "," + to_string(destCol) + ")";
                    historialMovimientos.push_back(registro);

                    mensajeEstado = "¡Pieza enemiga capturada!";
                    cambiarTurno();
                    return true;
                } else {
                    mensajeEstado = "[Error] No hay ficha enemiga para capturar.";
                    return false;
                }
            } else {
                mensajeEstado = "[Error] Movimiento diagonal no valido.";
                return false;
            }
        }
    }

    void verificarCoronacion(int fila, int col) {
        if (tablero[fila][col] == CLARA && fila == 0) {
            tablero[fila][col] = DAMA_CLARA;
            mensajeEstado = "*** ¡Ficha Clara coronada como DAMA! ***";
        }
        if (tablero[fila][col] == OSCURA && fila == 7) {
            tablero[fila][col] = DAMA_OSCURA;
            mensajeEstado = "*** ¡Ficha Oscura coronada como DAMA! ***";
        }
    }

    void renderizar(int tiempoRestante) {
        limpiarYRedibujar();

        cout << GRID_COLOR << "=====================================================\n" << RESET;
        cout << " TURNO: " << (turnoClaras ? CLARA_COLOR : OSCURA_COLOR) 
             << (turnoClaras ? "JUGADOR CLARAS (C)" : "JUGADOR OSCURAS (O)") << RESET;
        cout << " | TIEMPO: " << (tiempoRestante <= 5 ? "\033[1;31m" : "\033[1;32m") 
             << setfill('0') << setw(2) << tiempoRestante << "s" << RESET << "\n";
        cout << GRID_COLOR << "=====================================================\n" << RESET;
        cout << GRID_COLOR << "        0    1    2    3    4    5    6    7\n";
        cout << "     +----+----+----+----+----+----+----+----+\n" << RESET;

        for (int i = 0; i < 8; ++i) {
            cout << GRID_COLOR << "  " << i << "  |" << RESET;
            for (int j = 0; j < 8; ++j) {
                TipoFicha f = tablero[i][j];
                string estilo = "";
                string contenido = "    ";

                if (f == CLARA) contenido = " C  ";
                else if (f == OSCURA) contenido = " O  ";
                else if (f == DAMA_CLARA) contenido = " DC ";
                else if (f == DAMA_OSCURA) contenido = " DO ";
                else if ((i + j) % 2 == 0) contenido = " #  ";

                if (i == cursorFila && j == cursorCol) {
                    estilo = CURSOR_COLOR;
                } else if (i == selFila && j == selCol) {
                    estilo = SELECCION_COLOR;
                } else {
                    if (f == CLARA) estilo = CLARA_COLOR;
                    else if (f == OSCURA) estilo = OSCURA_COLOR;
                    else if (f == DAMA_CLARA) estilo = DAMA_CLARA_COLOR;
                    else if (f == DAMA_OSCURA) estilo = DAMA_OSCURA_COLOR;
                    else estilo = GRID_COLOR;
                }

                cout << estilo << contenido << RESET << GRID_COLOR << "|" << RESET;
            }
            cout << "\n     +----+----+----+----+----+----+----+----+\n";
        }

        cout << "\n[ESTADO]: " << mensajeEstado << "\n";

        cout << GRID_COLOR << "-----------------------------------------------------\n" << RESET;
        cout << "\033[1;35mHISTORIAL DE MOVIMIENTOS EN LA PARTIDA:\033[0m\n";
        if (historialMovimientos.empty()) {
            cout << "  (Aun no se han realizado movimientos)\n";
        } else {
            int inicio = std::max(0, (int)historialMovimientos.size() - 5);
            for (size_t k = inicio; k < historialMovimientos.size(); ++k) {
                cout << "  " << k + 1 << ". " << historialMovimientos[k] << "\n";
            }
        }
        cout << GRID_COLOR << "-----------------------------------------------------\n" << RESET;
        cout << "Controles: [W,A,S,D] Mover | [ESPACIO/ENTER] Seleccionar | [Q] Salir\n";
        
        fflush(stdout);
    }

    bool esJuegoTerminado() {
        bool hayClaras = false, hayOscuras = false;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                if (tablero[i][j] == CLARA || tablero[i][j] == DAMA_CLARA) hayClaras = true;
                if (tablero[i][j] == OSCURA || tablero[i][j] == DAMA_OSCURA) hayOscuras = true;
            }
        }
        if (!hayClaras) {
            limpiarYRedibujar();
            cout << "\n========================================\n";
            cout << OSCURA_COLOR << " ¡FIN DEL JUEGO! ¡LAS OSCURAS HAN GANADO! \n" << RESET;
            cout << "========================================\n";
            return true;
        }
        if (!hayOscuras) {
            limpiarYRedibujar();
            cout << "\n========================================\n";
            cout << CLARA_COLOR << " ¡FIN DEL JUEGO! ¡LAS CLARAS HAN GANADO!  \n" << RESET;
            cout << "========================================\n";
            return true;
        }
        return false;
    }
};

int main() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif

    activarPantallaModoJuego();

    TableroDamas juego;
    auto inicioTurno = chrono::steady_clock::now();
    int ultimoTiempoMostrado = -1;

    juego.renderizar(TIEMPO_TURNO_SEGUNDOS);

    while (!juego.esJuegoTerminado()) {
        auto ahora = chrono::steady_clock::now();
        int transcurrido = chrono::duration_cast<chrono::seconds>(ahora - inicioTurno).count();
        int tiempoRestante = TIEMPO_TURNO_SEGUNDOS - transcurrido;

        if (tiempoRestante <= 0) {
            juego.forzarCambioPorTiempo();
            inicioTurno = chrono::steady_clock::now();
            tiempoRestante = TIEMPO_TURNO_SEGUNDOS;
            ultimoTiempoMostrado = tiempoRestante;
            juego.renderizar(tiempoRestante);
            continue;
        }

        bool redibujar = false;

        if (_kbhit()) {
            int tecla = _getch();

            if (tecla == 'q' || tecla == 'Q') {
                break;
            }

            if (tecla == 'w' || tecla == 'W' || tecla == 's' || tecla == 'S' ||
                tecla == 'a' || tecla == 'A' || tecla == 'd' || tecla == 'D') {
                juego.moverCursor(tecla);
                redibujar = true;
            } 
            else if (tecla == ' ' || tecla == 13 || tecla == 10) {
                bool movido = juego.procesarAccion();
                if (movido) {
                    inicioTurno = chrono::steady_clock::now();
                    tiempoRestante = TIEMPO_TURNO_SEGUNDOS;
                    ultimoTiempoMostrado = tiempoRestante;
                }
                redibujar = true;
            }
        }

        if (tiempoRestante != ultimoTiempoMostrado) {
            ultimoTiempoMostrado = tiempoRestante;
            redibujar = true;
        }

        if (redibujar) {
            juego.renderizar(tiempoRestante);
        }

#ifdef _WIN32
        Sleep(20);
#else
        usleep(20000);
#endif
    }

    desactivarPantallaModoJuego();
    cout << "\nJuego finalizado correctamente.\n";

    return 0;
}
