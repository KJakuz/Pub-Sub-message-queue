Publish–Subscribe Message Queue

Opis:
Projekt składa się z trzech elementów: serwera (message broker), współdzielonej biblioteki klienta oraz demonstracyjnego programu używającego tej biblioteki. Program ma za zadanie pozwolić na wymianę wiadomości między klientami z użyciem serwera jako brokera wiadomości.

Działanie demonstracyjnego programu:
Klient łączy się do serwera i wysyła swój identyfikator (jeśli identyfikator zajęty, klient zmuszony jest podać inny).
Klient po połączeniu się z serwerem ma do wyboru tryb subskrybenta lub wydawcy.
Opcja subskrybenta pozwala na:
        • Zasubskrybowanie kolejek dostępnych na serwerze (dostępnych tj. wcześniej stworzonych przez wydawców w tej sesji uruchomienia serwera).
        • Odbieranie nadchodzących aktualnie komunikatów z kolejek które subskrybent śledzi.
        • Rezygnację z subskrypcji.
Opcja wydawcy pozwala na:
        • Stworzenie nowej kolejki.
        • Nadanie komunikatu w dowolnej już istniejącej kolejce dostępnej na serwerze z określeniem czasu ważności wiadomości (TTL).
        • Usunięcie kolejki.

Funkcjonalność serwera (message broker):
Serwer przy każdej zmianie stanu którejkolwiek kolejki aktualizuje informacje i rozsyła je dla klientów. Na przykład przy pojawieniu się nowej wiadomości w kolejce X każdy subskrybent tej kolejki otrzyma tą wiadomość. Jeśli nowa kolejka zostanie utworzona lub usunięta, wtedy przy tej operacji każdy połączony klient dostanie zaktualizowane dane o kolejkach.
Obsługa konfliktów: W przypadkach konfliktu, np. gdy dwóch wydawców spróbuje stworzyć kolejkę o takiej samej nazwie, serwer nie pozwoli na to i zapisze tylko kolejkę od pierwszego wydawcy.
Usuwanie kolejek: W momencie gdy subskrybent nasłuchuje kolejkę, którą ktoś chce usunąć, serwer informuje drugą stronę o zakończeniu istnienia kolejki. Jeśli wydawca usunie kolejkę która ma subskrybentów, serwer odsubskrybuje za nich tą kolejkę.
Zarządzanie stanem klientów: Broker może śledzić stan klientów i przy rozłączeniu klienta jego subskrypcje pamiętane są przez pewien czas, w razie ponownego połączenia.
Time To Live (TTL) wiadomości: Każda wiadomość publikowana przez wydawcę ma określony czas ważności (TTL). Serwer automatycznie usuwa wygasłe wiadomości z kolejek. Nowo przyłączający się subskrybenci otrzymują tylko te wiadomości, które jeszcze nie wygasły.

Architektura projektu:
        1. Message Broker (serwer) - zarządza kolejkami, przechowuje wiadomości, rozsyła komunikaty do subskrybentów, usuwa wygasłe wiadomości.
        2. Współdzielona biblioteka klienta - udostępnia API do komunikacji z brokerem:
                • Łączenie z serwerem
                • Tworzenie/usuwanie kolejek
                • Publikowanie wiadomości z TTL
                • Subskrybowanie/odsubskrybowanie kolejek
                • Odbieranie wiadomości
        3. Program demonstracyjny - pokazuje jak używać biblioteki w obu trybach (publisher i subscriber).

Projekt wykonywany będzie bez GUI.
