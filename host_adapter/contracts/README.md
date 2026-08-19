# contracts/ — procedencia

`AudioModuleContract.h` en este directorio es una **copia exacta, sin
modificar**, de `app/src/main/cpp/modules/AudioModuleContract.h` del
proyecto `OlyzeAudioModuleHost` (el Host que carga módulos de audio
vía `dlopen()`).

## Por qué está vendorizado en vez de referenciado

`host_adapter/` es un target de build independiente (no depende de
`OlyzeAudioModuleHost` como submódulo ni como dependencia de ningún
tipo) para poder compilarse y probarse sin necesitar el repositorio
del Host presente. El propio comentario del contrato lo dice
explícitamente: es "lo único en lo que un módulo y el Host están de
acuerdo" — precisamente por eso debe poder vivir como una copia
autocontenida a cada lado.

## Regla de sincronización

Si `AudioModuleContract.h` cambia en `OlyzeAudioModuleHost`, esta
copia queda desactualizada y **el Host, no este archivo, es la fuente
de verdad**. Antes de asumir que `host_adapter/` sigue siendo
compatible con una versión nueva del Host:

1. Compara este archivo byte a byte contra la versión actual del Host.
2. Si `OLYZE_MODULE_ABI_VERSION_MAJOR` cambió, el adaptador necesita
   revisión — el Host rechazará cargar el módulo si no coincide.
3. Si solo cambiaron partes menores (nuevos tipos de evento, nuevos
   campos opcionales), revisa `olyze_module_adapter.cpp` para decidir
   si vale la pena soportarlos.

## Última sincronización conocida

Copiado el mismo día en que se recibió `OlyzeAudioModuleHost.zip` del
usuario, sin ninguna modificación (verificado con `diff` antes de
usarse como base del adaptador).
