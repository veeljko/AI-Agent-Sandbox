# Krabsetw NativeExamples - detaljan vodic

Ovaj dokument objasnjava sve sto ti je potrebno da citas i razumes primere iz:

```text
Project3\krabsetw\examples\NativeExamples
```

Dokument je podeljen u dva velika dela:

1. Zajednicki pojmovi i najbitnije funkcije biblioteke `krabsetw`.
2. Objasnjenje svakog primera iz `NativeExamples`, jedan po jedan.

Svi primeri koriste nativni C++ API iz:

```cpp
#include "..\..\krabs\krabs.hpp"
```

U tvom `Project3\main.cpp` koristi se ista biblioteka, samo sa include putanjom:

```cpp
#include "krabsetw/krabs/krabs.hpp"
```

## 1. Velika slika: sta NativeExamples pokazuje?

`NativeExamples` pokazuje kako se preko `krabsetw` cita ETW.

ETW je **Event Tracing for Windows**. To je Windows sistem za emitovanje i
slusanje dogadjaja. Dogadjaje emituju:

- aplikacije
- Windows servisi
- PowerShell
- .NET runtime
- WinINet
- kernel
- file system
- registry
- security auditing

`krabsetw` je wrapper oko niskonivojskog ETW API-ja. Umesto da rucno radis sa
Windows strukturama, dobijas C++ klase:

- `krabs::user_trace`
- `krabs::kernel_trace`
- `krabs::provider`
- `krabs::kernel_provider`
- `krabs::event_filter`
- `krabs::schema`
- `krabs::parser`
- `krabs::testing::record_builder`

Najkraci mentalni model:

```text
trace session -> provider -> callback -> schema -> parser -> polja eventa
```

Drugim recima:

1. Napravis trace sesiju.
2. Dodas jednog ili vise providera.
3. Svakom provideru das callback funkciju.
4. Kada event stigne, napravis `schema`.
5. Iz `schema` citas metapodatke.
6. Preko `parser` citas konkretna polja eventa.

## 2. Kako se pokrecu primeri?

Fajl:

```text
NativeExamples\main.cpp
```

samo bira koji primer se pokrece:

```cpp
int main(void)
{
    kernel_and_user_trace_001::start();
    //kernel_trace_001::start();
    //kernel_trace_002::start();
    //...
}
```

Ideja je da odkomentarises samo jedan primer koji zelis da probas.

Bitna napomena: fajl `kernel_and_user_trace_001.cpp` u ovoj verziji foldera ima
svoj `int main()`, a ne `kernel_and_user_trace_001::start()`. To znaci da je
napisan kao samostalan primer. Ostali primeri uglavnom prate obrazac
`ime_primera::start()`.

## 3. User trace i kernel trace

Postoje dve glavne vrste trace sesija.

### 3.1 `krabs::user_trace`

Koristi se za non-kernel providere, npr:

- `Microsoft-Windows-PowerShell`
- `Microsoft-Windows-WinINet`
- `Microsoft-Windows-DotNETRuntime`
- `Microsoft-Windows-Kernel-Process` kada se slusa kao user provider
- `Microsoft-Windows-Security-Auditing`

Primer:

```cpp
krabs::user_trace trace;
```

Ili imenovana sesija:

```cpp
krabs::user_trace trace(L"My Named Trace");
```

Ime trace sesije moze pomoci pri debugovanju. Ako ne das ime, biblioteka moze
generisati svoje.

### 3.2 `krabs::kernel_trace`

Koristi se za kernel ETW providere. Kernel trace obicno zahteva Administrator
terminal.

Primer:

```cpp
krabs::kernel_trace trace(L"My Kernel Trace");
```

Kernel trace lici na user trace:

```cpp
trace.enable(provider);
trace.start();
trace.stop();
```

Ali se razlikuje po tome koje providere mozes da koristis i kako Windows
omogucava te dogadjaje.

## 4. Provider

Provider je izvor eventa.

### 4.1 Provider preko GUID-a

PowerShell provider:

```cpp
krabs::provider<> provider(
    krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
);
```

Ovaj GUID predstavlja PowerShell ETW provider.

### 4.2 Provider preko imena

Neki primeri koriste ime:

```cpp
krabs::provider<> provider(L"Microsoft-Windows-WinINet");
```

Ili:

```cpp
krabs::provider<> provider(L"Microsoft-Windows-Kernel-Process");
```

Oba pristupa su cest obrazac. GUID je precizniji i uvek direktno odgovara ETW
provideru. Ime je citljivije kada ga biblioteka/Windows moze razresiti.

### 4.3 Kernel convenience provideri

Za neke kernel izvore `krabsetw` vec ima gotove klase:

```cpp
krabs::kernel::image_load_provider provider;
krabs::kernel::process_provider provider;
krabs::kernel::disk_file_io_provider provider;
krabs::kernel::object_manager_provider provider;
```

Ove klase kriju neke detalje oko kernel flagova.

### 4.4 Generic kernel provider

Ako nemas convenience klasu, mozes koristiti:

```cpp
krabs::kernel_provider provider(GUID_NULL, PERF_REG_HIVE);
```

Ili:

```cpp
krabs::kernel_provider provider(0, krabs::guids::event_trace_config);
```

To je napredniji oblik i najvise se vidi u kernel primerima.

## 5. `any()` i `all()` flagovi

ETW provider ima keyword/flag filtere. Oni odredjuju koje kategorije eventa
provider emituje.

### 5.1 `any`

```cpp
provider.any(0xf0010000000003ff);
```

Znacenje vrednosti zavisi od providera. Na PowerShell provideru ova vrednost
ukljucuje veliki skup PowerShell dogadjaja.

Za process provider:

```cpp
provider.any(0x10);
```

U primerima komentar kaze da je `0x10` `WINEVENT_KEYWORD_PROCESS`.

### 5.2 `all`

```cpp
provider.all(0x4000000000000000);
```

`all` znaci da event mora da zadovolji sve trazene keyword uslove. U praksi,
semantika `any` i `all` zavisi od konkretnog ETW providera.

### 5.3 Kako da nadjes prave flagove?

Koristi Windows alat:

```powershell
wevtutil gp Microsoft-Windows-PowerShell /ge /gm:true
```

Ili listu providera:

```powershell
wevtutil ep
```

Za trazenje:

```powershell
wevtutil ep | Select-String PowerShell
```

## 6. Callback funkcija

Callback je funkcija koju `krabsetw` poziva kada event stigne.

Tipican potpis:

```cpp
[](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
    // obrada eventa
}
```

Ili:

```cpp
void callback(const EVENT_RECORD& record, const krabs::trace_context& trace_context)
{
}
```

Parametri:

- `EVENT_RECORD& record` je sirova Windows ETW struktura.
- `trace_context` sadrzi kontekst koji `krabsetw` koristi za schema lookup.

Callback se dodaje na provider:

```cpp
provider.add_on_event_callback(callback);
```

Ili na filter:

```cpp
filter.add_on_event_callback(callback);
```

Vazno: callback treba da bude brz. Ako callback radi sporo, trace moze poceti
da gubi evente jer se novi eventi ne skidaju dovoljno brzo iz ETW buffera.

## 7. `krabs::schema`

`schema` je objekat koji ti govori sta je stiglo.

Pravi se ovako:

```cpp
krabs::schema schema(record, trace_context.schema_locator);
```

Najbitnije funkcije:

```cpp
schema.event_id();
schema.event_name();
schema.event_opcode();
schema.opcode_name();
schema.event_version();
schema.event_level();
schema.provider_name();
schema.task_name();
schema.process_id();
schema.stack_trace();
```

Koristi `schema` kada hoces:

- da filtriras event po ID-ju
- da vidis ime eventa
- da vidis opcode
- da ispises provider/task/opcode informacije
- da izvuces stack trace ako je ukljucen

Primer:

```cpp
std::wcout << L"Event " << schema.event_id();
std::wcout << L" (" << schema.event_name() << L") received." << std::endl;
```

## 8. `krabs::parser`

`parser` cita konkretna polja iz payload-a eventa.

Pravi se iz `schema`:

```cpp
krabs::parser parser(schema);
```

Citanje stringa:

```cpp
std::wstring context = parser.parse<std::wstring>(L"ContextInfo");
```

Citanje broja:

```cpp
uint32_t pid = parser.parse<uint32_t>(L"ProcessID");
```

Citanje ANSI stringa:

```cpp
std::string image = parser.parse<std::string>(L"ImageFileName");
```

Ako trazis polje koje ne postoji ili koristis pogresan tip, `parse` moze baciti
exception. Zato je za istrazivanje korisno:

```cpp
try {
    krabs::parser parser(schema);
    auto value = parser.parse<std::wstring>(L"SomeField");
    std::wcout << value << std::endl;
} catch (const std::exception& ex) {
    std::cout << "Parse failed: " << ex.what() << std::endl;
}
```

## 9. `event_filter`

`event_filter` filtrira evente pre callback-a.

### 9.1 Filter preko predicate funkcije

```cpp
krabs::event_filter filter(krabs::predicates::id_is(7937));
```

Ovaj filter propusta samo evente ciji je ID `7937`.

### 9.2 Filter preko event ID-ja

```cpp
krabs::event_filter filter(11);
```

Ovaj oblik koristi ETW API event ID filtering. U primerima se koristi za file
delete event.

### 9.3 Filter preko event ID-ja plus predicate

```cpp
krabs::event_filter filter(11, krabs::predicates::process_id_is(4));
```

Ovo prvo trazi event ID `11`, pa dodatno propusta samo dogadjaje iz procesa sa
PID `4`.

### 9.4 Dodavanje filtera na provider

```cpp
provider.add_filter(filter);
```

Callback mozes staviti direktno na provider ili na filter:

```cpp
provider.add_on_event_callback(...); // dobija sve evente providera
filter.add_on_event_callback(...);   // dobija samo filtrirane evente
```

## 10. Predicate funkcije

Predicate je funkcija/objekat koji vraca `true` ili `false` za event.

Primeri iz NativeExamples:

```cpp
krabs::predicates::id_is(7937)
krabs::predicates::process_id_is(4)
krabs::predicates::opcode_is(1)
krabs::predicates::version_is(3)
```

Za kombinovanje vise uslova:

```cpp
krabs::predicates::any_of({ &p1, &p2, &p3 })
```

Znacenje:

- `any_of`: dovoljno je da jedan uslov bude tacan.
- `all_of`: svi uslovi moraju biti tacni.
- `none_of`: nijedan uslov ne sme biti tacan.

U `user_trace_006_predicate_vectors.cpp` prikazan je `any_of`.

## 11. `trace.enable()`, `trace.start()`, `trace.stop()`

### 11.1 `enable`

Provider se dodaje trace sesiji:

```cpp
trace.enable(provider);
```

Jedna trace sesija moze imati vise providera:

```cpp
trace.enable(ps_provider);
trace.enable(wininet_provider);
```

### 11.2 `start`

```cpp
trace.start();
```

`start()` blokira trenutni thread. To znaci da program tu ostaje dok se trace ne
zaustavi.

Ako hoces da program radi jos nesto dok trace slusa evente, pokreni trace u
posebnom thread-u:

```cpp
std::thread t([&trace]() {
    trace.start();
});

Sleep(10000);
trace.stop();
t.join();
```

### 11.3 `stop`

```cpp
trace.stop();
```

Zaustavlja trace sesiju. Obicno se koristi kada je `start()` pokrenut u drugom
thread-u ili kada citas iz `.etl` fajla.

## 12. `set_trace_filename()`

`set_trace_filename()` se koristi kada ne zelis real-time trace, nego citas vec
snimljen ETL fajl:

```cpp
trace.set_trace_filename(L"..\\..\\examples\\NativeExamples\\powershell.etl");
trace.start();
trace.stop();
```

U tom rezimu `start()` cita evente iz fajla i zavrsava kada dodje do kraja fajla.

## 13. Rundown events

Rundown eventi opisuju postojece stanje sistema, a ne samo nove dogadjaje.

Primeri:

- trenutno pokrenuti procesi
- trenutno ucitani .NET assembly-ji
- otvoreni fajlovi
- hardware configuration podaci

Neki provideri emituju rundown evente kada trace pocne, neki kada trace stane.

Za neke user providere treba:

```cpp
provider.enable_rundown_events();
```

Za .NET runtime postoji poseban provider:

```cpp
Microsoft-Windows-DotNETRuntimeRundown
```

## 14. Stack trace eventi

Da bi ETW event nosio call stack, provider mora da se ukljuci sa posebnim
trace flagom:

```cpp
provider.trace_flags(
    provider.trace_flags() | EVENT_ENABLE_PROPERTY_STACK_TRACE
);
```

Zatim se stack cita:

```cpp
auto stack_trace = schema.stack_trace();
for (auto& return_address : stack_trace) {
    std::wcout << L"0x" << std::hex << return_address << std::endl;
}
```

Ovo daje adrese, ne imena funkcija. Da bi video simbole/funkcije, potreban je
dodatni symbol resolution korak koji ovi primeri ne pokrivaju.

## 15. Direktan rad sa `EVENT_RECORD`

Ponekad primeri koriste sirovi `EVENT_RECORD` direktno:

```cpp
record.EventHeader.EventDescriptor.Opcode
record.EventHeader.ProcessId
record.EventHeader.ProviderId
```

To je korisno kada hoces nesto sto je vec u ETW header-u, bez dodatnog
parsiranja payload-a.

`schema` je zgodniji i citljiviji za vecinu stvari, ali `EVENT_RECORD` je
originalni izvor podataka.

## 16. Pregled svih primera

U folderu su ovi glavni primeri:

```text
kernel_and_user_trace_001.cpp
kernel_trace_001.cpp
kernel_trace_002.cpp
kernel_trace_003_rundown.cpp
multiple_providers_001.cpp
testing_001.cpp
user_trace_001.cpp
user_trace_002.cpp
user_trace_003_no_predicates.cpp
user_trace_004.cpp
user_trace_005.cpp
user_trace_006_predicate_vectors.cpp
user_trace_007_rundown.cpp
user_trace_008_stacktrace.cpp
user_trace_009_from_file.cpp
```

`examples.h` samo deklarise `start()` funkcije, a `main.cpp` bira koji primer se
pokrece.

## 17. `user_trace_001.cpp`

Tema: osnovni user trace za PowerShell.

Ovaj primer:

1. Pravi `krabs::user_trace`.
2. Pravi PowerShell provider preko GUID-a.
3. Ukljucuje PowerShell keyword flagove preko `provider.any(...)`.
4. Dodaje callback direktno na provider.
5. Za svaki event ispisuje ID i ime.
6. Ako je event ID `7937`, cita polje `ContextInfo`.
7. Pokrece trace preko `trace.start()`.

Kljucni kod:

```cpp
krabs::user_trace trace;
krabs::provider<> provider(
    krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
);
provider.any(0xf0010000000003ff);
```

Callback:

```cpp
provider.add_on_event_callback([](const EVENT_RECORD& record,
                                  const krabs::trace_context& trace_context) {
    krabs::schema schema(record, trace_context.schema_locator);
    std::wcout << L"Event " << schema.event_id();
});
```

Parsiranje:

```cpp
krabs::parser parser(schema);
std::wstring context = parser.parse<std::wstring>(L"ContextInfo");
```

Sta treba da naucis iz ovog primera:

- osnovni oblik user trace programa
- provider preko GUID-a
- callback direktno na provider
- `schema.event_id()`
- `schema.event_name()`
- `parser.parse<T>()`

## 18. `user_trace_002.cpp`

Tema: provider-level filtering pomocu predicate-a.

Razlika u odnosu na `user_trace_001`:

- `user_trace_001` dobija sve evente pa rucno proverava `if (event_id == 7937)`.
- `user_trace_002` pravi `event_filter` koji propusta samo event ID `7937`.

Kljucni kod:

```cpp
krabs::event_filter filter(krabs::predicates::id_is(7937));
```

Callback se dodaje na filter:

```cpp
filter.add_on_event_callback([](...) {
    krabs::schema schema(record, trace_context.schema_locator);
    assert(schema.event_id() == 7937);
});
```

Filter se dodaje na provider:

```cpp
provider.add_filter(filter);
```

Sta treba da naucis:

- `event_filter` moze da ima svoj callback
- `krabs::predicates::id_is()` filtrira po event ID-ju
- filtriranje u `krabsetw` cini callback jednostavnijim

## 19. `user_trace_003_no_predicates.cpp`

Tema: ETW event ID filter bez predicate funkcije.

Ovaj primer slusa file event provider:

```cpp
krabs::provider<> provider(
    krabs::guid(L"{EDD08927-9CC4-4E65-B970-C2560FB5C289}")
);
```

Zatim pravi filter:

```cpp
krabs::event_filter filter(11);
```

Komentar kaze da je ovo file delete event. Bitna razlika je sto ovaj oblik
filtera moze biti prosledjen ETW tracing API-ju kao event ID filter, umesto da
sve evente prvo primi aplikacija pa ih onda odbaci.

Callback:

```cpp
assert(schema.event_id() == 11);
std::wcout << L"Event " + std::to_wstring(schema.event_id()) + L" received!";
```

Sta treba da naucis:

- `event_filter(11)` filtrira po event ID-ju
- nije svaki filter samo C++ post-processing
- file event provider se moze slusati kao user provider

## 20. `user_trace_004.cpp`

Tema: kombinovanje ETW event ID filtera i predicate-a.

Ovaj primer je slican prethodnom, ali dodaje uslov da event mora biti iz procesa
sa PID `4`, sto je System process.

Kljucni kod:

```cpp
krabs::event_filter filter(11, krabs::predicates::process_id_is(4));
```

Znacenje:

- event ID mora biti `11`
- `process_id` mora biti `4`

Callback proverava oba uslova:

```cpp
assert(schema.event_id() == 11);
assert(schema.process_id() == 4);
```

Sta treba da naucis:

- filter moze imati vise nivoa
- `schema.process_id()` cita process ID iz event metadata
- predicate moze filtrirati po procesu

## 21. `user_trace_005.cpp`

Tema: Security Auditing provider i posebne privilegije.

Ovaj primer slusa:

```cpp
Microsoft-Windows-Security-Auditing
```

To je provider koji puni Windows Security Event Log. Poseban je jer ne moze da
ga slusa obican korisnik, a ni obican Administrator nije dovoljan za ovaj nacin
traceovanja. Primer proverava da li program radi kao `SYSTEM`:

```cpp
char user_name[128] = { 0 };
DWORD user_name_length = 128;
if (!GetUserNameA(user_name, &user_name_length) || !strcmp(user_name, "SYSTEM") == 0)
{
    std::wcout << L"... can only be traced by SYSTEM" << std::endl;
    return;
}
```

Zatim otvara postojecu session:

```cpp
krabs::user_trace trace(L"EventLog-Security");
```

Ovaj trace session vec pravi OS, pa ga primer ne pravi od nule.

Provider:

```cpp
krabs::provider<> provider(L"Microsoft-Windows-Security-Auditing");
```

Za event ID `4703`, cita:

```cpp
EnabledPrivilegeList
DisabledPrivilegeList
```

Sta treba da naucis:

- neki provideri imaju posebna security ogranicenja
- nije svaki ETW provider jednako dostupan
- trace session moze biti postojeca Windows session
- `provider.any((ULONGLONG)-1)` ovde ne menja stvarno flagove jer je provider
  poseban

## 22. `user_trace_006_predicate_vectors.cpp`

Tema: kombinovanje predicate-a preko `any_of`.

Provider:

```cpp
krabs::provider<> provider(L"Microsoft-Windows-Kernel-Process");
provider.any(0x10);
```

Definisu se tri predicate-a:

```cpp
krabs::predicates::opcode_is opcode_is_1 =
    krabs::predicates::opcode_is(1);

krabs::predicates::id_is eventid_is_2 =
    krabs::predicates::id_is(2);

krabs::predicates::version_is version_is_3 =
    krabs::predicates::version_is(3);
```

Zatim se kombinuju:

```cpp
krabs::event_filter filter(
    krabs::predicates::any_of({
        &opcode_is_1,
        &eventid_is_2,
        &version_is_3,
    })
);
```

Znacenje: propusti event ako je tacan bar jedan od uslova.

Vazna C++ napomena: predicate objekti se cuvaju u lokalnim promenljivama, a
`any_of` dobija pokazivace na njih. Zato moraju ziveti dovoljno dugo dok se
filter koristi.

Sta treba da naucis:

- predicate-i mogu biti objekti
- `any_of` kombinuje vise uslova
- slicno postoje `all_of` i `none_of`
- filtriranje moze biti po ID-ju, opcode-u i verziji

## 23. `user_trace_007_rundown.cpp`

Tema: rundown events u user trace svetu.

Rundown eventi opisuju trenutno stanje, ne samo nove dogadjaje.

Prvi deo prati process provider:

```cpp
krabs::provider<> process_provider(L"Microsoft-Windows-Kernel-Process");
process_provider.any(0x10);
process_provider.enable_rundown_events();
```

`enable_rundown_events()` salje provideru signal da emituje state/rundown
evente.

Callback cita:

```cpp
uint32_t pid = parser.parse<uint32_t>(L"ProcessID");
std::wstring image_name = parser.parse<std::wstring>(L"ImageName");
```

Zatim pravi dva filtera:

```cpp
krabs::event_filter process_filter(krabs::predicates::id_is(1));
krabs::event_filter process_rundown_filter(krabs::predicates::id_is(15));
```

Znacenje:

- ID `1`: real-time ProcessStart
- ID `15`: ProcessRundown, tj. procesi koji vec postoje

Drugi deo pokazuje .NET runtime i .NET rundown provider:

```cpp
krabs::provider<> dotnet_provider(L"Microsoft-Windows-DotNETRuntime");
krabs::provider<> dotnet_rundown_provider(L"Microsoft-Windows-DotNETRuntimeRundown");
```

Real-time assembly load:

```cpp
dotnet_provider.any(0x8);
krabs::event_filter assembly_filter(krabs::predicates::id_is(154));
```

Rundown assembly eventi:

```cpp
dotnet_rundown_provider.any(0x8 | 0x40);
krabs::event_filter assembly_rundown_filter(krabs::predicates::id_is(155));
```

Polje koje cita:

```cpp
FullyQualifiedAssemblyName
```

Sta treba da naucis:

- rundown event = slika trenutnog stanja
- neki provideri imaju rundown evente u istom provideru
- neki imaju poseban rundown provider
- `enable_rundown_events()` je potreban za neke scenarije

## 24. `user_trace_008_stacktrace.cpp`

Tema: prikupljanje call stack-a uz event.

Provider:

```cpp
krabs::provider<> process_provider(L"Microsoft-Windows-Kernel-Process");
process_provider.any(0x10);
```

Ukljucivanje stack trace-a:

```cpp
process_provider.trace_flags(
    process_provider.trace_flags() | EVENT_ENABLE_PROPERTY_STACK_TRACE
);
```

Filter:

```cpp
krabs::event_filter process_filter(krabs::predicates::id_is(1));
```

To znaci da se prate ProcessStart eventi.

Callback cita:

```cpp
auto pid = parser.parse<uint32_t>(L"ProcessID");
auto image_name = parser.parse<std::wstring>(L"ImageName");
auto stack_trace = schema.stack_trace();
```

Ispisuje povratne adrese:

```cpp
for (auto& return_address : stack_trace)
{
    std::wcout << L"   0x" << std::hex << return_address << std::endl;
}
```

Sta treba da naucis:

- stack trace mora posebno da se ukljuci
- `schema.stack_trace()` vraca adrese
- primer ne radi symbol resolution
- stack trace moze povecati kolicinu podataka i trosak obrade

## 25. `user_trace_009_from_file.cpp`

Tema: citanje eventa iz `.etl` fajla umesto real-time tracing-a.

Ovaj primer je skoro isti kao `user_trace_001`, ali dodaje:

```cpp
trace.set_trace_filename(L"..\\..\\examples\\NativeExamples\\powershell.etl");
```

To znaci:

- trace ne slusa live sistem
- cita snimljeni ETL fajl
- `trace.start()` zavrsava kada se fajl procita do kraja

Na kraju:

```cpp
trace.stop();
```

Sta treba da naucis:

- ETW eventi mogu biti snimljeni u `.etl`
- isti callback/parsing model radi i za live i za file input
- `set_trace_filename()` prebacuje trace u file mode

## 26. `multiple_providers_001.cpp`

Tema: vise user providera u jednoj trace sesiji.

Primer pravi jedan `user_trace`:

```cpp
krabs::user_trace trace;
```

Zatim dva providera:

```cpp
krabs::provider<> ps_provider(
    krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
);
krabs::provider<> wininet_provider(L"Microsoft-Windows-WinINet");
```

Svaki provider dobija svoj setup:

```cpp
setup_ps_provider(ps_provider);
setup_wininet_provider(wininet_provider);
```

Oba se enable-uju na isti trace:

```cpp
trace.enable(ps_provider);
trace.enable(wininet_provider);
```

PowerShell deo je isti kao ranije.

WinINet deo koristi:

```cpp
provider.all(0x4000000000000000);
```

Za event ID `1057`, cita:

```cpp
URL
RequestHeaders
ResponseHeaders
```

Sta treba da naucis:

- jedna trace sesija moze slusati vise providera
- svaki provider moze imati svoj callback
- u callback-u znas sta je stiglo preko `schema`
- za mrezne evente cesto se parsiraju ANSI stringovi (`std::string`)

## 27. `kernel_trace_001.cpp`

Tema: osnovni kernel trace, image load eventi.

Trace:

```cpp
krabs::kernel_trace trace(L"My magic trace");
```

Provider:

```cpp
krabs::kernel::image_load_provider provider;
```

Callback proverava opcode:

```cpp
if (schema.event_opcode() == 10) {
    krabs::parser parser(schema);
    std::wstring filename = parser.parse<std::wstring>(L"FileName");
}
```

Znacenje: kada se ucita executable ili DLL, ispisuje se `FileName`.

Sta treba da naucis:

- kernel trace koristi `krabs::kernel_trace`
- kernel provideri imaju svoje convenience klase
- kernel eventi se cesto razlikuju po opcode-u, ne samo po event ID-ju
- za kernel trace uglavnom treba Administrator

## 28. `kernel_trace_002.cpp`

Tema: manje dokumentovani kernel eventi i default callback.

Ovaj primer pokazuje dve naprednije stvari.

Prvo, object manager provider:

```cpp
krabs::kernel::object_manager_provider ob_provider;
```

Callback gleda opcode `33`:

```cpp
if (record.EventHeader.EventDescriptor.Opcode == 33) {
    krabs::schema schema(record, trace_context.schema_locator);
    krabs::parser parser(schema);
    std::wstring name = parser.parse<std::wstring>(L"ObjectName");
}
```

Ako se ime zavrsava na `dll`, ispisuje da je handle zatvoren.

Drugo, generic kernel provider preko PERFINFO group mask:

```cpp
krabs::kernel_provider hive_provider(GUID_NULL, PERF_REG_HIVE);
```

Za takve dogadjaje ponekad nisi siguran koji provider GUID ce stvarno stici.
Zato primer koristi default callback:

```cpp
trace.set_default_event_callback([](...) {
    krabs::schema schema(record, trace_context.schema_locator);
    std::wcout << schema.provider_name();
});
```

Default callback hvata evente koji nemaju specifican registered provider callback.

Sta treba da naucis:

- neki kernel eventi se enable-uju preko PERFINFO group mask
- `set_default_event_callback()` je dobar za istrazivanje
- mozes citati `record.EventHeader.ProviderId`
- nisu svi kernel eventi lepo dokumentovani

## 29. `kernel_trace_003_rundown.cpp`

Tema: kernel rundown eventi koji se emituju na pocetku i kraju trace-a.

Ovaj primer koristi tri vrste informacija:

1. Process rundown
2. File rundown
3. Hardware configuration

### Process rundown

Provider:

```cpp
krabs::kernel::process_provider process_provider;
```

Callback:

```cpp
process_provider.add_on_event_callback(process_rundown_callback);
```

Funkcija `process_rundown_callback` gleda opcode:

```cpp
if (record.EventHeader.EventDescriptor.Opcode == 3 ||
    record.EventHeader.EventDescriptor.Opcode == 4)
```

To su DCStart/DCEnd rundown eventi. Cita:

```cpp
ProcessId
ImageFileName
```

### File rundown

Provider:

```cpp
krabs::kernel::disk_file_io_provider fileio_provider;
```

Callback gleda opcode `36`, sto komentar oznacava kao `FileRundown`.

Cita:

```cpp
FileName
```

### Hardware configuration

Provider:

```cpp
krabs::kernel_provider hwconfig_provider(0, krabs::guids::event_trace_config);
```

Callback propusta samo nekoliko opcode-a:

```cpp
10 // CPU
25 // Platform
33 // DeviceFamily
37 // Boot Config Info
```

### Start/stop preko thread-a

Primer mora da startuje trace pa ga zaustavi, zato koristi thread:

```cpp
std::thread thread([&trace]() { trace.start(); });
Sleep(1500);
trace.stop();
thread.join();
```

Ovo je potrebno jer se neki rundown eventi emituju kada trace pocne, a neki
kada trace prestaje.

Sta treba da naucis:

- kernel rundown eventi opisuju stanje sistema
- start i stop trace-a mogu sami proizvesti evente
- `trace.start()` je blocking, pa se koristi `std::thread`
- parser moze citati i `std::string` i `std::wstring`

## 30. `kernel_and_user_trace_001.cpp`

Tema: user trace i kernel trace u istom programu.

Ovaj fajl je specifican jer ima svoj:

```cpp
int main()
```

umesto `kernel_and_user_trace_001::start()`.

Pravi dve trace sesije:

```cpp
krabs::user_trace user;
krabs::kernel_trace kernel;
```

Pravi dva providera:

```cpp
krabs::provider<> ps_provider(
    krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}")
);
krabs::kernel::image_load_provider image_load_provider;
```

Podesavanje je izvuceno u funkcije:

```cpp
setup_ps_provider(ps_provider);
setup_image_load_provider(image_load_provider);
```

Zatim:

```cpp
user.enable(ps_provider);
kernel.enable(image_load_provider);
```

Posto `start()` blokira, oba trace-a se startuju u posebnim thread-ovima:

```cpp
std::thread user_thread([&user]() { user.start(); });
std::thread kernel_thread([&kernel]() { kernel.start(); });
```

Program ceka 10 sekundi:

```cpp
Sleep(10000);
```

Zatim zaustavlja oba trace-a:

```cpp
user.stop();
kernel.stop();
```

I ceka da thread-ovi zavrse:

```cpp
user_thread.join();
kernel_thread.join();
```

Sta treba da naucis:

- jedan program moze imati vise trace sesija
- svaka trace sesija trosi thread kada radi
- user trace i kernel trace mogu raditi paralelno
- `start()` se obicno stavlja u thread ako treba kontrolisan stop

## 31. `testing_001.cpp`

Tema: testiranje callback koda bez pravog ETW trace-a.

Ovaj primer pokazuje `krabsetw` testing API.

Pravi obican user trace i PowerShell provider:

```cpp
krabs::user_trace trace(L"My Named Trace");
krabs::provider<> powershellProvider(powershell);
```

Dodaje callback koji parsira:

```cpp
ContextInfo
```

Ali ne zove:

```cpp
trace.start();
```

Umesto toga pravi proxy:

```cpp
krabs::testing::user_trace_proxy proxy(trace);
```

Zatim pravi fake event:

```cpp
krabs::testing::record_builder builder(
    powershell,
    krabs::id(7937),
    krabs::version(1)
);
```

Dodaje fake property-je:

```cpp
builder.add_properties()
    (L"ContextInfo", L"Some silly test values here")
    (L"Data", L"Some other data here");
```

Pakuje event:

```cpp
auto record = builder.pack_incomplete();
```

I gura ga kroz trace:

```cpp
proxy.push_event(record);
```

Razlika izmedju `pack()` i `pack_incomplete()`:

- `pack()` ocekuje da popunis sva polja koja schema trazi
- `pack_incomplete()` dozvoljava da popunis samo neka polja, a ostala defaultuje

Sta treba da naucis:

- callback mozes testirati bez realnog ETW eventa
- `record_builder` pravi fake `EVENT_RECORD`
- `user_trace_proxy` salje fake event kroz postojeci trace setup
- ovo je korisno za unit testove

## 32. `main.cpp` u NativeExamples

Ovaj fajl nije primer ETW API-ja sam po sebi. On je launcher.

Sadrzi:

```cpp
int main(void)
{
    // Comment in/out the particular example you'd like to run.
    kernel_and_user_trace_001::start();
    //kernel_trace_001::start();
    //...
}
```

Namena:

- odkomentarises primer koji zelis
- zakomentarises ostale
- build-ujes projekat
- pokrenes executable

Napomena za ovu kopiju: posto `kernel_and_user_trace_001.cpp` ima svoj
`int main()`, moze doci do konflikta ako build ukljuci i `NativeExamples\main.cpp`
i `kernel_and_user_trace_001.cpp` u isti executable bez prilagodjavanja.

## 33. Najbitnije funkcije i sta rade

### `krabs::user_trace trace;`

Pravi ETW trace sesiju za user-mode providere.

### `krabs::user_trace trace(L"name");`

Pravi imenovanu user trace sesiju.

### `krabs::kernel_trace trace(L"name");`

Pravi kernel trace sesiju. Cesto treba Administrator.

### `krabs::provider<> provider(L"ProviderName");`

Pravi user provider preko imena.

### `krabs::provider<> provider(krabs::guid(L"{...}"));`

Pravi user provider preko GUID-a.

### `provider.any(flags);`

Ukljucuje evente koji odgovaraju bilo kom od trazenih keyword flagova.

### `provider.all(flags);`

Ukljucuje evente koji zadovoljavaju sve trazene keyword flagove.

### `provider.add_on_event_callback(callback);`

Dodaje callback za sve evente koje provider propusti.

### `trace.enable(provider);`

Dodaje provider u trace session.

### `trace.start();`

Pokrece slusanje. Blokira thread.

### `trace.stop();`

Zaustavlja trace.

### `trace.set_trace_filename(path);`

Umesto live ETW-a cita evente iz `.etl` fajla.

### `trace.set_default_event_callback(callback);`

Postavlja callback za evente koji ne odgovaraju konkretnom provider callback-u.
Korisno za istrazivanje kernel eventa.

### `provider.enable_rundown_events();`

Trazi od providera da emituje rundown/state evente.

### `provider.trace_flags(...)`

Cita ili postavlja ETW enable property flagove. U primerima se koristi za
`EVENT_ENABLE_PROPERTY_STACK_TRACE`.

### `krabs::event_filter filter(predicate);`

Pravi filter zasnovan na predicate-u.

### `krabs::event_filter filter(event_id);`

Pravi filter po event ID-ju, gde se ID moze proslediti ETW API-ju.

### `krabs::event_filter filter(event_id, predicate);`

Kombinuje event ID filter i dodatni predicate.

### `filter.add_on_event_callback(callback);`

Dodaje callback koji se poziva samo za evente koji prodju filter.

### `provider.add_filter(filter);`

Kaci filter na provider.

### `krabs::schema schema(record, trace_context.schema_locator);`

Ucita schema informacije za event.

### `schema.event_id()`

Vraca event ID.

### `schema.event_name()`

Vraca ime eventa, ako je dostupno.

### `schema.event_opcode()`

Vraca opcode. Posebno vazno za kernel evente.

### `schema.opcode_name()`

Vraca tekstualno ime opcode-a, ako postoji.

### `schema.task_name()`

Vraca ime task-a u ETW manifestu.

### `schema.provider_name()`

Vraca ime providera.

### `schema.process_id()`

Vraca process ID iz event metadata.

### `schema.stack_trace()`

Vraca listu return address vrednosti ako je stack trace ukljucen.

### `krabs::parser parser(schema);`

Pravi parser za payload eventa.

### `parser.parse<T>(L"FieldName")`

Cita polje iz event payload-a kao tip `T`.

Primeri:

```cpp
parser.parse<std::wstring>(L"ContextInfo");
parser.parse<std::wstring>(L"FileName");
parser.parse<std::string>(L"ImageFileName");
parser.parse<uint32_t>(L"ProcessID");
```

### `krabs::predicates::id_is(x)`

Predicate koji proverava event ID.

### `krabs::predicates::process_id_is(x)`

Predicate koji proverava process ID.

### `krabs::predicates::opcode_is(x)`

Predicate koji proverava opcode.

### `krabs::predicates::version_is(x)`

Predicate koji proverava event version.

### `krabs::predicates::any_of(...)`

Kombinuje predicate-e tako da je dovoljan jedan tacan uslov.

### `krabs::testing::user_trace_proxy`

Testing helper koji moze da ubaci fake event u trace bez realnog ETW slusanja.

### `krabs::testing::record_builder`

Testing helper za pravljenje fake `EVENT_RECORD`.

### `builder.add_properties()`

Dodaje payload vrednosti u fake event.

### `builder.pack()`

Pakuje fake event i ocekuje sva potrebna polja.

### `builder.pack_incomplete()`

Pakuje fake event i dozvoljava da neka polja fale.

### `proxy.push_event(record)`

Salje fake event kroz callback pipeline.

## 34. Kako da citas novi ETW primer

Kada otvoris novi primer, prati ovaj red:

1. Nadji koji trace se koristi:

```cpp
krabs::user_trace
krabs::kernel_trace
```

2. Nadji providere:

```cpp
krabs::provider<>
krabs::kernel::...
krabs::kernel_provider
```

3. Pogledaj flagove:

```cpp
provider.any(...)
provider.all(...)
provider.trace_flags(...)
provider.enable_rundown_events()
```

4. Nadji callback:

```cpp
add_on_event_callback(...)
```

5. U callback-u prvo gledaj filter uslov:

```cpp
schema.event_id()
schema.event_opcode()
record.EventHeader.EventDescriptor.Opcode
```

6. Nadji parser:

```cpp
krabs::parser parser(schema);
parser.parse<T>(L"Field")
```

7. Na kraju pogledaj kako trace pocinje i staje:

```cpp
trace.start()
trace.stop()
std::thread
```

## 35. Sta treba posebno zapamtiti

`trace.start()` blokira. Ako zelis kontrolisan stop, koristi thread.

`provider.any()` i `provider.all()` zavise od konkretnog providera. Ista
vrednost ne znaci isto za sve providere.

`schema` je za metapodatke eventa, `parser` je za payload polja.

`event_filter` moze smanjiti broj eventa koji callback mora da obradi.

Kernel trace obicno trazi Administrator privilegije.

Security auditing primer trazi `SYSTEM`, sto je jace od obicnog Administratora.

Rundown eventi opisuju postojece stanje sistema.

Stack trace daje adrese, ne automatski imena funkcija.

ETL fajlovi se mogu citati istim callback modelom kao live eventi.

Testing API ti dozvoljava da testiras callback bez realnog eventa.

## 36. Predlozen redosled ucenja primera

Najlakse je ovim redom:

1. `user_trace_001.cpp`
2. `user_trace_002.cpp`
3. `user_trace_003_no_predicates.cpp`
4. `user_trace_004.cpp`
5. `multiple_providers_001.cpp`
6. `user_trace_009_from_file.cpp`
7. `testing_001.cpp`
8. `kernel_trace_001.cpp`
9. `kernel_trace_003_rundown.cpp`
10. `user_trace_007_rundown.cpp`
11. `user_trace_008_stacktrace.cpp`
12. `user_trace_006_predicate_vectors.cpp`
13. `kernel_trace_002.cpp`
14. `user_trace_005.cpp`
15. `kernel_and_user_trace_001.cpp`

Razlog: prvo naucis osnovni user trace, zatim filtere, pa vise providera, zatim
file/testing, pa kernel, rundown, stacktrace i tek onda security/generic kernel
naprednije stvari.

