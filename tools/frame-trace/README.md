# frame-trace — medir el frame step en ejecución

Herramientas para reproducir las medidas publicadas en
[`docs/FRAME_STEP_TRACE.md`](../../docs/FRAME_STEP_TRACE.md): cuántas veces se
ejecuta cada función del update por frame, en qué orden y con qué argumentos.

No se distribuye ningún binario del juego. El trazador se compila aquí y se
ejecuta contra **tu propia copia** de `GEX3D.EXE` (EU, MD5
`692b12825003417cc4a5a9d4db13eebe`).

## Por qué breakpoints de hardware

El primer intento fue un depurador clásico con `INT3` (`0xCC`) y single-step.
El juego muere con `0xC000041D` / `0xC0000005` dentro de sus callbacks de tiempo
antes del primer frame de juego: el parcheo de código y el reentro constante en
el depurador rompen el temporizador. Los registros de depuración `Dr0`–`Dr3`
disparan por ejecución de dirección sin tocar un solo byte, y el juego corre
normal. El precio es **cuatro direcciones por sesión**.

## Compilar

Windows x64, .NET Framework 4 (viene con Windows):

```bat
%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe /nologo /platform:x64 ^
  /optimize+ /out:tracer.exe tracer.cs
```

El host es x64 y el objetivo WOW64 (32 bits): los registros se leen con
`Wow64GetThreadContext`.

## Ejecutar

```bat
tracer.exe <exe> <workdir> <log> <timeoutMs> hw:<addr,...> <keys|none>
```

| Argumento | Significado |
| --- | --- |
| `exe` | ruta a `GEX3D.EXE` (copia de trabajo, nunca la instalación original) |
| `workdir` | directorio de trabajo del proceso (donde están `LEVEL`, `MOVIE`, …) |
| `log` | fichero TSV de salida |
| `timeoutMs` | tope de la sesión en milisegundos |
| `hw:` | hasta 4 direcciones relativas a la imagen, en hex (`hw:43e1ba,540650`) |
| `keys` | códigos de tecla virtual en hex, separados por comas, uno cada 2 s desde los 8 s (`1B` Escape, `0D` Enter, `28` Abajo, `20` Espacio) |

Ejemplo mínimo (ancla del frame + tres sitios del update):

```bat
tracer.exe C:\gex-run\GEX3D.EXE C:\gex-run trace.log 45000 ^
  hw:43e1ba,540650,465530,43e719 1B,1B,1B,1B,0D,0D,28,0D,0D,20,0D
```

Las teclas se inyectan con `PostMessage` a la ventana del juego (se descarta la
`D3DProxyWindow` que crea nGlide). `Escape` salta las FMV; el bucle de cine solo
cuenta `WM_KEYDOWN` de `VK_ESCAPE` y clics, así que `Enter` no sirve ahí.

## Leer el log

```bat
node summarize.mjs trace.log
```

```text
# trace.log  frameSteps=467 hits=1938
  0x0043E719 <- 0x0043E2FA  calls=467 frames=467/467
  0x00465530 <- 0x0043E2EE  calls=467 frames=467/467
  0x00540650 <- 0x0043E28D  calls=467 frames=467/467
```

- **Ancla:** cada sesión debe incluir `0x0043E1BA` (entrada del update). Se llama
  exactamente una vez por frame, así que el flujo de hits se parte en frames con
  denominador exacto.
- **`calls`**: hits totales. **`frames`**: frames en los que apareció al menos una
  vez. Una función que corre una vez por frame da `calls == frames`.
- **`<- ret`**: la dirección de retorno que el `call` dejó en la pila. Distingue
  dos sitios que llaman a la misma función (p. ej. `0x45E5C5` desde `0x43E3EB` y
  desde `0x43E4F5`).
- **Cola truncada:** si la captura termina justo después del ancla, el último
  frame queda abierto y los sitios muestran `n-1/n`. Descontando ese frame, todos
  aparecen una vez por frame completo.

## Formato del log

```text
BP  seq  ms  tid  addr  esp  ret  a0  a1  a2
```

`ms` es el reloj del trazador desde `CreateProcess`, `tid` el hilo, `esp` la pila
en el momento del hit y `a0`–`a2` los tres primeros argumentos. Las líneas que
empiezan por `#` son metadatos (base de imagen, breakpoints, teclas, salida).

## Límites conocidos

- Una sesión mide **cuatro** direcciones. Las tablas de 24 callees se obtienen
  encadenando sesiones, cada una con el ancla incluida.
- Solo se observa lo que se ejecuta: una rama no tomada no aparece, y eso es un
  resultado (negativo), no un fallo de la herramienta.
- El juego se ejecuta bajo nGlide; el temporizador y el swap son los del wrapper.
  Las conclusiones sobre *orden* y *frecuencia por frame* no dependen de eso; las
  de tiempo absoluto sí.
- El trazador no restaura nada del proceso: lo termina al acabar. Usa siempre una
  copia de trabajo del ejecutable, nunca la instalación que quieras conservar.
