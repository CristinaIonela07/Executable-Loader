# Executable-Loader

Pentru inceput am verificat daca adresa page fault-ului se afla in 
afara tuturor segmentelor si astfel puteam sa rulez din prima handler-ul 
default.

Am parcurs pe rand fiecare segment si am alocat memorie pentru campul 
de date (al fiecarui segment) doar daca acesta era null. Daca adresa 
fault-ului se gaseste intr-un segment din executabil si pagina nu a fost 
mapata, se mapeaza pagina in memorie si se copiaza datele din segmentul 
din fisier in memorie, urmand setarea permisiunilor paginii mapate cu 
permisiunile segmentului.

Daca page fault-ul este generat intr-o pagina deja mapata, inseamna ca 
se incearca un acces nepermis la memorie si se ruleaza handler-ul default
de page fault.
