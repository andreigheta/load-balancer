/* Copyright 2023 <Gheţa Andrei-Cristian> */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "load_balancer.h"
#include "utils.h"

unsigned int hash_function_servers(void *a) {
	unsigned int uint_a = *((unsigned int *)a);

	uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
	uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
	uint_a = (uint_a >> 16u) ^ uint_a;
	return uint_a;
}

load_balancer *init_load_balancer() {
	load_balancer *main = (load_balancer *)malloc(sizeof(load_balancer));
	DIE(!main, "Malloc failed");

	main->hash_ring = NULL;
	main->nr_servers = 0;

	return main;
}

// sorteaza serverele in functie de hash-ul lor
// daca au acelasi hash, sunt sortate in functie de id
void sort_servers(load_balancer *main) {
	for (int i = 0; i < main->nr_servers - 1; i++)
		for (int j = i + 1; j < main->nr_servers; j++)
			if (main->hash_ring[i].hash > main->hash_ring[j].hash) {
				server_memory tmp = main->hash_ring[i];
				main->hash_ring[i] = main->hash_ring[j];
				main->hash_ring[j] = tmp;
			} else if (main->hash_ring[i].hash == main->hash_ring[j].hash) {
				if (main->hash_ring[i].server_id > main->hash_ring[j].server_id) {
					server_memory tmp = main->hash_ring[i];
					main->hash_ring[i] = main->hash_ring[j];
					main->hash_ring[j] = tmp;
				}
			}
}

// sunt hashuite id-urile server-ului principal si al clonelor acestuia
void add_server_hashes(load_balancer *main, int server_id) {
	main->hash_ring[main->nr_servers].server_id = server_id;
	main->hash_ring[main->nr_servers].hash = hash_function_servers(&server_id);

	int tag1 = pow(10, 5) + server_id;
	int tag2 = 2 * pow(10, 5) + server_id;

	unsigned int clone1_hash = hash_function_servers(&tag1);
	unsigned int clone2_hash = hash_function_servers(&tag2);

	main->hash_ring[main->nr_servers + 1].hash = clone1_hash;
	main->hash_ring[main->nr_servers + 2].hash = clone2_hash;

	main->hash_ring[main->nr_servers + 1].server_id = server_id;
	main->hash_ring[main->nr_servers + 2].server_id = server_id;
}

// elementele sunt distribuite la locul bun
void distribute_elements(load_balancer *main, int server_id) {
	for (int i = 0; i < main->nr_servers; i++) {
		if (main->hash_ring[i].server_id == server_id) {
			int nr_information = 0;

			// in caz ca este ultimul server, redistribuim elementele
			// de pe primul
			if (i == main->nr_servers - 1) {
				// primeste toate informatiile din server-ul ale carui elemente
				// trebuie mutate
				info *information = server_retrieve_all(&main->hash_ring[0],
														&nr_information);
				for (int j = 0; j < nr_information; j++) {
					server_remove(&main->hash_ring[0], information[j].key);
					int id;
					loader_store(main, information[j].key, information[j].value, &id);
					free(information[j].key);
					free(information[j].value);
				}
				free(information);
			} else {
				info *information = server_retrieve_all(&main->hash_ring[i + 1],
														&nr_information);
				for (int j = 0; j < nr_information; j++) {
					server_remove(&main->hash_ring[i + 1], information[j].key);
					int id;
					loader_store(main, information[j].key, information[j].value, &id);
					free(information[j].key);
					free(information[j].value);
				}
				free(information);
			}
		}
	}
}

// initializeaza serverele
void initialize_servers(load_balancer *main) {
	// de fiecare data cand este adaugat un server, vor fi alocate
	// 3 servere, adica cel principal + 2 clone
	server_memory *server1 = init_server_memory();
	server_memory *server2 = init_server_memory();
	server_memory *server3 = init_server_memory();

	// copiem datele pe serverele de pe hashring
	memcpy(&main->hash_ring[main->nr_servers], server1, sizeof(server_memory));
	memcpy(&main->hash_ring[main->nr_servers + 1], server2, sizeof(server_memory));
	memcpy(&main->hash_ring[main->nr_servers + 2], server3, sizeof(server_memory));

	// sunt eliberate cele de pe care am copiat datele
	free(server1);
	free(server2);
	free(server3);
}

void loader_add_server(load_balancer *main, int server_id) {
	// daca este primul server adaugat
	if (!main->hash_ring) {
		main->hash_ring = (server_memory *)malloc(3 * sizeof(server_memory));
		DIE(!main->hash_ring, "Malloc failed");

		// sunt initializate
		initialize_servers(main);

		// sunt hashuite
		add_server_hashes(main, server_id);

		main->nr_servers += 3;

		// sunt sortate
		sort_servers(main);

		return;
	}

	main->hash_ring = realloc(main->hash_ring,
							 (main->nr_servers + 3) * sizeof(server_memory));
	DIE(!main->hash_ring, "Realloc failed");

	// sunt initializate
	initialize_servers(main);

	// sunt hashuite
	add_server_hashes(main, server_id);

	main->nr_servers += 3;

	// sunt sortate
	sort_servers(main);

	// le sunt distribuite/redistribuite elementele corect
	distribute_elements(main, server_id);
}

// redistribuie elementele odata ce un server a fost sters
void move_elements(load_balancer *main, int server_id) {
	for (int i = 0; i < main->nr_servers; i++) {
		if (main->hash_ring[i].server_id == server_id) {
			int nr_information = 0;

			// primeste toate informatiile din server-ul care trebuie sters
			info *information = server_retrieve_all(&main->hash_ring[i],
													&nr_information);
			free_server_memory(&main->hash_ring[i]);

			// serverele sunt shiftate la stanga
			for (int k = i + 1; k < main->nr_servers; k++)
				main->hash_ring[k - 1] = main->hash_ring[k];

			main->nr_servers--;

			// punem informatia in server-ul bun
			for (int j = 0; j < nr_information; j++) {
				loader_store(main, information[j].key, information[j].value,
							 &server_id);
				free(information[j].key);
				free(information[j].value);
			}
			free(information);

			return;
		}
	}
}

void loader_remove_server(load_balancer *main, int server_id) {
	// pentru fiecare server sters va trebui sa stergem si serverele clone
	// deci vom realiza operatia de 3 ori
	move_elements(main, server_id);
	move_elements(main, server_id);
	move_elements(main, server_id);

	main->hash_ring = realloc(main->hash_ring,
							  main->nr_servers * sizeof(server_memory));
	DIE(!main->hash_ring, "Realloc failed");
}

void loader_store(load_balancer *main, char *key, char *value, int *server_id) {
	for (int i = 0; i < main->nr_servers; i++)
		if (main->hash_ring[i].hash > hash_function_key(key)) {
			server_store(&main->hash_ring[i], key, value);
			*server_id = main->hash_ring[i].server_id;

			return;
		}

	// in caz ca hash-ul key-ului este mai mare decat toate hash-urile
	// serverelor, atunci automat va fi asignat primului server
	server_store(&main->hash_ring[0], key, value);
	*server_id = main->hash_ring[0].server_id;
}

char *loader_retrieve(load_balancer *main, char *key, int *server_id) {
	char *information;
	for (int i = 0; i < main->nr_servers; i++) {
		if (main->hash_ring[i].hash > hash_function_key(key)) {
			information = server_retrieve(&main->hash_ring[i], key);
			*server_id = main->hash_ring[i].server_id;

			return information;
		}
	}

	// in caz ca hash-ul key-ului este mai mare decat toate hash-urile
	// serverelor si primul server exista, atunci automat va fi asignat
	// primului server
	if (server_has_key(&main->hash_ring[0], key)) {
		information = server_retrieve(&main->hash_ring[0], key);
		*server_id = main->hash_ring[0].server_id;

		return information;
	}

	return NULL;
}

void free_load_balancer(load_balancer *main) {
	for (int i = 0; i < main->nr_servers; i++)
		free_server_memory(&main->hash_ring[i]);

	free(main->hash_ring);
	free(main);
}
