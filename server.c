/* Copyright 2023 <Gheţa Andrei-Cristian> */
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "utils.h"

#define MAX_STRING_SIZE	256
#define HMAX 256

linked_list_t *ll_create(unsigned int data_size)
{
    linked_list_t* ll;

    ll = malloc(sizeof(*ll));
	DIE(!ll, "Malloc failed");

    ll->head = NULL;
    ll->data_size = data_size;
    ll->size = 0;

    return ll;
}

/*
 * Pe baza datelor trimise prin pointerul new_data, se creeaza un nou nod care e
 * adaugat pe pozitia n a listei reprezentata de pointerul list. Pozitiile din
 * lista sunt indexate incepand cu 0 (i.e. primul nod din lista se afla pe
 * pozitia n=0). Daca n >= nr_noduri, noul nod se adauga la finalul listei. Daca
 * n < 0, eroare.
 */
void ll_add_nth_node(linked_list_t* list, unsigned int n, const void* new_data)
{
    ll_node_t *prev, *curr;
    ll_node_t* new_node;

    if (!list) {
        return;
    }

    /* n >= list->size inseamna adaugarea unui nou nod la finalul listei. */
    if (n > list->size) {
        n = list->size;
    }

    curr = list->head;
    prev = NULL;
    while (n > 0) {
        prev = curr;
        curr = curr->next;
        --n;
    }

    new_node = malloc(sizeof(*new_node));
	DIE(!new_node, "Malloc failed");
    new_node->data = malloc(list->data_size);
	DIE(!new_node->data, "Malloc failed");
    memcpy(new_node->data, new_data, list->data_size);

    new_node->next = curr;
    if (prev == NULL) {
        /* Adica n == 0. */
        list->head = new_node;
    } else {
        prev->next = new_node;
    }

    list->size++;
}

/*
 * Elimina nodul de pe pozitia n din lista al carei pointer este trimis ca
 * parametru. Pozitiile din lista se indexeaza de la 0 (i.e. primul nod din
 * lista se afla pe pozitia n=0). Daca n >= nr_noduri - 1, se elimina nodul de
 * la finalul listei. Daca n < 0, eroare. Functia intoarce un pointer spre acest
 * nod proaspat eliminat din lista. Este responsabilitatea apelantului sa
 * elibereze memoria acestui nod.
 */
ll_node_t *ll_remove_nth_node(linked_list_t* list, unsigned int n)
{
    ll_node_t *prev, *curr;

    if (!list || !list->head) {
        return NULL;
    }

    /* n >= list->size - 1 inseamna eliminarea nodului de la finalul listei. */
    if (n > list->size - 1) {
        n = list->size - 1;
    }

    curr = list->head;
    prev = NULL;
    while (n > 0) {
        prev = curr;
        curr = curr->next;
        --n;
    }

    if (prev == NULL) {
        /* Adica n == 0. */
        list->head = curr->next;
    } else {
        prev->next = curr->next;
    }

    list->size--;

    return curr;
}

/*
 * Functia intoarce numarul de noduri din lista al carei pointer este trimis ca
 * parametru.
 */
unsigned int ll_get_size(linked_list_t* list)
{
     if (!list) {
        return -1;
    }

    return list->size;
}

/*
 * Procedura elibereaza memoria folosita de toate nodurile din lista, iar la
 * sfarsit, elibereaza memoria folosita de structura lista si actualizeaza la
 * NULL valoarea pointerului la care pointeaza argumentul (argumentul este un
 * pointer la un pointer).
 */
void ll_free(linked_list_t** pp_list)
{
    ll_node_t* curr;

    if (!pp_list || !*pp_list) {
        return;
    }

    while (ll_get_size(*pp_list) > 0) {
        curr = ll_remove_nth_node(*pp_list, 0);
        free(curr->data);
        curr->data = NULL;
        free(curr);
        curr = NULL;
    }

    free(*pp_list);
    *pp_list = NULL;
}

unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *)a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

// initializare server
server_memory *init_server_memory()
{
	server_memory *server = (server_memory *)malloc(sizeof(server_memory));
	DIE(!server, "Malloc failed");
    server->size = 0;

    server->buckets = (linked_list_t **)malloc(HMAX * sizeof(linked_list_t *));
	DIE(!server->buckets, "Malloc failed");

    for (unsigned int i = 0; i < HMAX; i++)
        server->buckets[i] = ll_create(sizeof(info));

    return server;
}

// verifica daca server-ul are un anumit key
int server_has_key(server_memory *server, char *key)
{
	if (!server)
		return -1;

    int hash = hash_function_key(key) % HMAX;
    ll_node_t *curr = server->buckets[hash]->head;

	while (curr != NULL) {
        if (!strcmp(((info *)curr->data)->key, key))
            return 1;
        curr = curr->next;
    }

	return 0;
}

// stocarea unei chei-valoare intr-un server
void server_store(server_memory *server, char *key, char *value) {
	if (!server)
		return;

	int hash = hash_function_key(key) % HMAX;
	ll_node_t *curr = server->buckets[hash]->head;

	if (server_has_key(server, key)) {
		free(server_retrieve(server, key));

		while (curr != NULL) {
			if (!strcmp(((info *)curr->data)->key, key))
				break;
			curr = curr->next;
		}

		((info *)curr->data)->value = malloc(strlen(value) + 1);
		DIE(!((info *)curr->data)->value, "Malloc failed");
		memcpy(((info *)curr->data)->value, value, strlen(value) + 1);
	} else {
		info *information = (info *)malloc(sizeof(info));
		DIE(!information, "Malloc failed");

		information->key = malloc(strlen(key) + 1);
		DIE(!information->key, "Malloc failed");

		information->value = malloc(strlen(value) + 1);
		DIE(!information->value, "Malloc failed");

		memcpy(information->key, key, strlen(key) + 1);
		memcpy(information->value, value, strlen(value) + 1);
		ll_add_nth_node(server->buckets[hash], 0, information);

        free(information);
		server->size++;
	}
}

// intoarce toate informatiile unui server
info *server_retrieve_all(server_memory *server, int *nr_information) {
    if (!server)
        return NULL;

    if (server->size == 0)
        return NULL;

    info *information = (info *)malloc(sizeof(info) * server->size);
    DIE(!information, "Malloc failed");

    int idx = 0;
    for (int i = 0; i < HMAX; i++) {
        ll_node_t *curr = server->buckets[i]->head;
        while (curr) {
            information[idx].key =
            malloc(strlen(((info *)curr->data)->key) + 1);
            DIE(!information[idx].key, "Malloc failed");

            information[idx].value =
            malloc(strlen(((info *)curr->data)->value) + 1);
            DIE(!information[idx].value, "Malloc failed");

            memcpy(information[idx].key, ((info *)curr->data)->key,
                   strlen(((info *)curr->data)->key) + 1);

            memcpy(information[idx].value, ((info *)curr->data)->value,
                   strlen(((info *)curr->data)->value) + 1);

            idx++;
            curr = curr->next;
        }
    }

    *nr_information = idx;

    return information;
}

// intoarce valoarea cheii de pe un server
char *server_retrieve(server_memory *server, char *key) {
	if (!server || !server_has_key(server, key))
		return NULL;

	int hash = hash_function_key(key) % HMAX;
    ll_node_t *curr = server->buckets[hash]->head;

	while (curr != NULL) {
        if (!strcmp(((info *)curr->data)->key, key))
            return ((info *)curr->data)->value;
        curr = curr->next;
    }

	return NULL;
}

// sterge un server
void server_remove(server_memory *server, char *key) {
	if (!server)
		return;

	int hash = hash_function_key(key) % HMAX;
	ll_node_t *curr = server->buckets[hash]->head;
	int count = 0;

	if (server_has_key(server, key)) {
		while (curr != NULL) {
			if (!strcmp(((info *)curr->data)->key, key))
				break;
			curr = curr->next;
			count++;
		}

		free(((info *)curr->data)->key);
        free(((info *)curr->data)->value);

		curr = ll_remove_nth_node(server->buckets[hash], count);
		free(curr->data);
		free(curr);
		server->size--;
	}
}

void free_server_memory(server_memory *server) {
	for (unsigned int i = 0; i < HMAX; i++) {
		ll_node_t *curr = server->buckets[i]->head;
		while (curr != NULL) {
            free(((info *)curr->data)->key);
            free(((info *)curr->data)->value);
			curr = curr->next;
		}
		ll_free(&server->buckets[i]);
	}
	free(server->buckets);
}
