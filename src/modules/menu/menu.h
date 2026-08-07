#ifndef MENU_H
#define MENU_H

#include <Arduino.h>

// Chame ao entrar no menu (ex.: quando o botão CONFIG for pressionado em
// AGUARDANDO_PEDIDO_PALAVRA). Reseta a navegação pro primeiro item e desenha.
void menuEntrar();

// Navegação (chamadas pelos botões remapeados enquanto estado == CONFIGURACAO)
void menuNavegarCima();
void menuNavegarBaixo();
void menuSelecionar();

// Função utilitária pra obter o PIN de pareamento atual, que é exibido no menu
String menuObterPinPareamento();

// Chame sempre no loop(), mas só enquanto estado == CONFIGURACAO. Detecta se
// algo que o item atual exibe mudou por fora (ex.: SSID alterado pelo portal
// web) e redesenha a tela na hora, sem precisar de nenhuma ação do usuário.
void menuTick();

#endif