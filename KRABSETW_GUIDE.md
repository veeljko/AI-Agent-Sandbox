# Krabsetw vodic za Project3

Ovaj fajl je praktican uvod u biblioteku `krabsetw`, napisan za program iz
`main.cpp`. Cilj nije da naucis sve o ETW-u odjednom, vec da razumes osnovni
obrazac i da znas kako da menjas program za druge evente.

## 1. Sta je krabsetw?

`krabsetw` je C++ biblioteka koja olaksava rad sa ETW-om.

ETW znaci **Event Tracing for Windows**. To je Windows mehanizam preko kog
operativni sistem i aplikacije emituju dogadjaje. Ti dogadjaji mogu da opisuju,
na primer:

- PowerShell komande
- pokretanje procesa
- ucitavanje DLL fajlova
- file system operacije
- security/audit dogadjaje
- mrezne dogadjaje

Bez `krabsetw`, ETW API je dosta niskog nivoa. `krabsetw` ti daje zgodnije C++
klase kao sto su:

- `krabs::user_trace`
- `krabs::kernel_trace`
- `krabs::provider`
- `krabs::schema`
- `krabs::parser`
- `krabs::event_filter`

## 2. Osnovni ETW recnik

**Provider** je izvor dogadjaja. Na primer, PowerShell ima svoj ETW provider.

**Trace session** je aktivna sesija koja slusa jednog ili vise providera.

**User trace** slusa dogadjaje iz aplikacija i Windows komponenti koje nisu
kernel provider. Tvoj program koristi `krabs::user_trace`.

**Kernel trace** slusa kernel dogadjaje, kao sto su process, thread, file i
image load dogadjaji. Obicno zahteva Administrator privilegije.

**Event** je jedan konkretan zapis koji provider emituje.

**Schema** opisuje strukturu eventa: ID, ime, opcode, level i polja koja event
sadrzi.

**Parser** cita konkretna polja iz eventa, na primer string, broj ili GUID.

## 3. Osnovni sablon programa

Vecina `krabsetw` programa izgleda ovako:

```cpp
#include <iostream>
#include "krabsetw/krabs/krabs.hpp"

int main() {
    krabs::user_trace trace;

    krabs::provider<> provider(
        krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
    );

    provider.any(0xf0010000000003ff);

    provider.add_on_event_callback(
        [](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
            krabs::schema schema(record, trace_context.schema_locator);

            std::wcout << L"Event " << schema.event_id()
                       << L" (" << schema.event_name() << L") received."
                       << std::endl;
        }
    );

    trace.enable(provider);
    trace.start();
}
```

Tok rada je:

1. Napravis trace sesiju.
2. Napravis provider.
3. Podesis flagove providera.
4. Dodas callback funkciju.
5. Omogucis provider na trace sesiji.
6. Pokrenes trace.

## 4. Sta radi tvoj trenutni `main.cpp`

Tvoj `main.cpp` slusa PowerShell provider:

```cpp
krabs::provider<> provider(
    krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
);
```

Ovaj GUID predstavlja `Microsoft-Windows-PowerShell`.

Zatim trazi veliki skup PowerShell dogadjaja:

```cpp
provider.any(0xf0010000000003ff);
```

Kada dogadjaj stigne, program ispisuje njegov ID i ime:

```cpp
std::wcout << L"Event " << schema.event_id();
std::wcout << L"(" << schema.event_name() << L") received." << std::endl;
```

Ako je event ID `7937`, program cita polje `ContextInfo`:

```cpp
if (schema.event_id() == 7937) {
    krabs::parser parser(schema);
    std::wstring context = parser.parse<std::wstring>(L"ContextInfo");
    std::wcout << L"\tContext: " << context << std::endl;
}
```

To znaci da te posebno zanimaju PowerShell dogadjaji koji nose dodatni kontekst
o izvrsavanju.

## 5. `user_trace` i `kernel_trace`

Za obicne Windows komponente i aplikacije koristi se:

```cpp
krabs::user_trace trace;
```

Moze i imenovana sesija:

```cpp
krabs::user_trace trace(L"My Named Trace");
```

Za kernel dogadjaje koristi se:

```cpp
krabs::kernel_trace trace(L"My Kernel Trace");
```

Primer kernel providera:

```cpp
krabs::kernel::image_load_provider provider;
```

Kernel trace cesto mora da se pokrene kao Administrator.

## 6. Provider

Provider mozes napraviti preko GUID-a:

```cpp
krabs::provider<> provider(
    krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
);
```

Ili, kod nekih providera, preko imena:

```cpp
krabs::provider<> provider(L"Microsoft-Windows-Security-Auditing");
```

Provider je ono sto odredjuje koje evente slusas.

## 7. `any` i `all` flagovi

ETW provideri imaju filter flagove. Najcesce ces koristiti `any`:

```cpp
provider.any(0xf0010000000003ff);
```

Znacenje flagova zavisi od konkretnog providera. Nema univerzalnog pravila da
flag `0x10` uvek znaci istu stvar za sve providere.

Za ucenje mozes prvo staviti sire flagove, ispisivati evente, pa kasnije
suzavati filter.

## 8. Callback funkcija

Callback je funkcija koja se poziva kada stigne event:

```cpp
provider.add_on_event_callback(
    [](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
        krabs::schema schema(record, trace_context.schema_locator);
        std::wcout << L"Event " << schema.event_id() << std::endl;
    }
);
```

Vazno: callback treba da bude brz. Ako u callback-u radis previse posla, ETW
moze poceti da gubi evente.

Dobro za callback:

- procitaj osnovna polja
- filtriraj ono sto ti treba
- ispisi ili prosledi podatak dalje

Lose za callback:

- dugo spavanje
- teska obrada
- mreza
- mnogo upisa u fajl bez buffer-a

## 9. Schema

`schema` ti daje informacije o eventu:

```cpp
krabs::schema schema(record, trace_context.schema_locator);
```

Korisne metode:

```cpp
schema.event_id();
schema.event_name();
schema.event_opcode();
schema.event_version();
schema.event_level();
```

Najcesce prvo ispises `event_id()` i `event_name()`, pa onda odlucis koje ID-jeve
zelis detaljnije da parsiras.

## 10. Parser

Kada znas da event ima odredjeno polje, citas ga preko `krabs::parser`:

```cpp
krabs::parser parser(schema);
std::wstring value = parser.parse<std::wstring>(L"ContextInfo");
```

Primer za brojeve:

```cpp
uint32_t pid = parser.parse<uint32_t>(L"ProcessId");
```

Primer za string:

```cpp
std::wstring name = parser.parse<std::wstring>(L"ImageName");
```

Ako polje ne postoji ili tip nije dobar, `parse` moze baciti exception. Za
eksperimentisanje je korisno uhvatiti gresku:

```cpp
try {
    std::wstring context = parser.parse<std::wstring>(L"ContextInfo");
    std::wcout << context << std::endl;
} catch (const std::exception& ex) {
    std::cout << "Parse failed: " << ex.what() << std::endl;
}
```

## 11. Filtriranje po event ID-ju

Najjednostavnije filtriranje je rucno u callback-u:

```cpp
if (schema.event_id() == 7937) {
    // parsiraj samo taj event
}
```

`krabsetw` podrzava i `event_filter`, gde filter kacis na providera:

```cpp
krabs::event_filter filter(7937);

filter.add_on_event_callback(
    [](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
        krabs::schema schema(record, trace_context.schema_locator);
        std::wcout << L"Filtered event " << schema.event_id() << std::endl;
    }
);

provider.add_filter(filter);
```

To je korisno kada zelis da callback dobija samo odredjeni event.

## 12. Kako pronaci providere i evente

Mozes koristiti Windows alat `wevtutil`.

Lista providera:

```powershell
wevtutil ep
```

Pretraga PowerShell providera:

```powershell
wevtutil ep | Select-String PowerShell
```

Detalji za provider:

```powershell
wevtutil gp Microsoft-Windows-PowerShell /ge /gm:true
```

Korisno je gledati:

- ime providera
- GUID
- event ID-jeve
- nazive polja
- keyword/flag vrednosti

## 13. Kako build-ovati tvoj projekat

U `Project3` vec imas `build-msvc.bat`. On podesava Visual Studio compiler i
linkuje potrebne Windows biblioteke:

- `tdh.lib`
- `advapi32.lib`
- `ole32.lib`

Primer build komande iz root foldera projekta:

```powershell
.\Project3\build-msvc.bat .\Project3\main.cpp .\Project3\main.exe
```

Pokretanje:

```powershell
.\Project3\main.exe
```

Za neke ETW izvore moraces da pokrenes terminal kao Administrator.

## 14. Mali zadaci za vezbu

### Zadatak 1: Ispisi samo event ID

U callback-u ostavi samo:

```cpp
std::wcout << L"Event ID: " << schema.event_id() << std::endl;
```

Pokreni program, zatim u drugom PowerShell prozoru izvrsi nekoliko komandi.

### Zadatak 2: Filtriraj jedan event

Dodaj:

```cpp
if (schema.event_id() != 7937) {
    return;
}
```

Tako ces ignorisati sve osim eventa koji te zanima.

### Zadatak 3: Dodaj try/catch oko parsera

Umesto direktnog `parse`, koristi:

```cpp
try {
    krabs::parser parser(schema);
    std::wstring context = parser.parse<std::wstring>(L"ContextInfo");
    std::wcout << L"Context: " << context << std::endl;
} catch (const std::exception& ex) {
    std::cout << "Parse failed: " << ex.what() << std::endl;
}
```

Ovo je korisno dok istrazujes nova polja.

### Zadatak 4: Dodaj brojac eventa

Pre callback-a:

```cpp
int count = 0;
```

U callback-u, ako koristis capture:

```cpp
provider.add_on_event_callback(
    [&count](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
        ++count;
        krabs::schema schema(record, trace_context.schema_locator);
        std::wcout << L"#" << count << L" Event " << schema.event_id() << std::endl;
    }
);
```

Napomena: za ozbiljniji multithreaded kod koristi `std::atomic<int>`.

## 15. Ceste greske

**Program se ne zavrsava.**

To je normalno. `trace.start()` blokira i slusa evente dok ne prekines program,
na primer sa `Ctrl+C`.

**Ne vidim evente.**

Moguci razlozi:

- provider ne emituje evente u tom trenutku
- flagovi nisu dobri
- treba Administrator terminal
- pogresan GUID ili ime providera
- PowerShell logging nije ukljucen za dogadjaj koji ocekujes

**Program pukne pri parsiranju.**

Najcesce polje ne postoji za taj event ID ili tip nije dobar. Prvo filtriraj
event ID, zatim parsiraj samo polja koja taj event stvarno ima.

**Ne znam nazive polja.**

Koristi:

```powershell
wevtutil gp Microsoft-Windows-PowerShell /ge /gm:true
```

Ili pogledaj postojece primere u:

```text
Project3\krabsetw\examples\NativeExamples
Project3\krabsetw\docs
```

## 16. Preporucen redosled ucenja

1. Razumi trenutni `main.cpp`.
2. Pokreni ga i gledaj koje event ID-jeve dobijas.
3. Nauci `schema.event_id()` i `schema.event_name()`.
4. Za jedan event pronadji njegova polja.
5. Parsiraj jedno polje.
6. Dodaj `try/catch`.
7. Probaj `event_filter`.
8. Tek onda probaj drugi provider.

## 17. Najkraci mentalni model

Zapamti ovaj lanac:

```text
trace -> provider -> callback -> schema -> parser -> polja eventa
```

Ako znas taj lanac, znas osnovu `krabsetw` biblioteke.
