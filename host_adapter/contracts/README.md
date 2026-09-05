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

Re-sincronizado tras recibir una versión más nueva de
`OlyzeAudioModuleHost.zip` que había bumpeado
`OLYZE_MODULE_ABI_VERSION_MAJOR` de 1 a 2 (agregó `structSize` como
primer campo de `OlyzeModuleDescriptor` — cambio real de layout
binario). La copia vendorizada aquí se había quedado en la versión
`MAJOR 1` mientras el Host ya exigía `MAJOR 2`, exactamente el
escenario de desincronización que esta regla describe: el Host
rechazaba el módulo con `OLYZE_ERR_UNSUPPORTED` ("Module descriptor is
smaller than this Host expects").

Fix aplicado:
1. `AudioModuleContract.h` reemplazado por copia byte-a-byte de la
   versión actual del Host (verificado con `diff`, cero diferencias).
2. `olyze_module_adapter.cpp`, en `olyze_module_entry()`: se agregó
   `d.structSize = sizeof(OlyzeModuleDescriptor);` en el mismo bloque
   donde ya se seteaba `d.abiVersionMajor`.

Verificado cargando el `.so` compilado con el `ModuleLoader.cpp` REAL
y sin modificar del Host (no una reimplementación del chequeo): antes
del fix, `ModuleLoader::load()` rechaza con `OLYZE_ERR_UNSUPPORTED`
(reproducido exactamente); después del fix, carga, prepara, procesa
un `NOTE_ON` con audio real, y descarga sin crash.
