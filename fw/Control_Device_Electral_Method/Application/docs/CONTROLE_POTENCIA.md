> Historical reference: this document describes the blocked delivery default.
> The current `Application/board/pinout.h` sets `BOARD_POWER_CONTROL_ENABLED=1`.
> The SD resolution preserves that setting; keep load/heater power disconnected
> during storage-only bench checks.

# Flag de potência — 15/10/2026

Em `Application/board/pinout.h`:

```c
#define BOARD_POWER_CONTROL_ENABLED 0
```

- **0 (padrão):** PD14 é mantido em relé OFF e PD12 em medição; chamadas de
  relé ON e aquecimento retornam false. Os pinos continuam como saídas
  seguras, não ficam flutuando nem são removidos da inicialização.
- **1:** após recompilar e gravar, permite os comandos de potência, somente
  após board_init e mediante os intertravamentos. Não liga no boot e não
  inicia um ensaio automaticamente. O menu mantém os pré-requisitos de
  cartão, ADC, sensor, parâmetros e calibração quando REQUIRE_CAL=1.

Sequência: `power_enable_measurement()` → `power_relay_on()` →
`power_select_heating()`. Para medir, `power_enable_measurement()` retorna
SELECT a HIGH. O ensaio já usa SELECT → 30 us → captura ADC.
`power_safe()` seleciona medição antes de desligar o relé.

O board também verifica a ordem para chamadas diretas, além do driver power:
não permite heating sem relé comandado ON, nem relé ON enquanto SELECT está
em heating. Falhas de timing/SPI retiram potência pelo caminho seguro.
O estado de saída é um comando de software; não existe feedback elétrico
do contato do relé ou confirmação física de corrente.

A flag habilita a implementação, não valida os tempos, a eletrônica ou o
protocolo. Ainda precisam ser medidos corrente de medição/aquecimento,
comutação, janela ADC, reset e latência de erro, conforme BANCADA.md.
As operações SD/HAL podem bloquear; os testes em computador não comprovam
resposta térmica nem substituem intertravamentos físicos.

A flag permanece em **0** nesta entrega. Macros de compilação `-D` também
podem sobrepor o valor; os testes usam isso para exercitar a versão 1.
Confira que não há override no CubeIDE antes de gravar a versão bloqueada.
