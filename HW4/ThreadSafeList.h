#ifndef THREAD_SAFE_LIST_H_
#define THREAD_SAFE_LIST_H_

#include <pthread.h>
#include <iostream>
#include <iomanip> // std::setw

using namespace std;

const unsigned int INITIAL_LIST_SIZE = 0;

template <typename T>
class List {
	public:
		/**
		* Constructor
		*/
		List() : size(INITIAL_LIST_SIZE) {
			head = new Node(T());
			pthread_mutex_init(&list_mutex, NULL);
		}

		/**
		* Destructor
		*/
		~List() {
			pthread_mutex_destroy(&list_mutex);
			pthread_mutex_destroy(&head->node_mutex);
			delete head;
		}

		class Node {
			public:
				T data;
				Node *next;
				pthread_mutex_t node_mutex;

				Node(T data) : data(data), next(NULL) {
					pthread_mutex_init(&node_mutex, NULL);
				}

		};

		/**
		* Insert new node to list while keeping the list ordered in an ascending order
		* If there is already a node has the same data as @param data then return false (without adding it again)
		* @param data the new data to be added to the list
		* @return true if a new node was added and false otherwise
		*/
		bool insert(const T& data)  {
			// lock dummy node
			Node *prev = head;
			pthread_mutex_lock(&prev->node_mutex);

			if (prev->next == NULL) {
				// in case list is empty
				Node* newNode = new Node(data);
				prev->next = newNode;
				pthread_mutex_lock(&list_mutex);
				size++;
				pthread_mutex_unlock(&list_mutex);
				__add_hook();
				pthread_mutex_unlock(&prev->node_mutex);
				return true;
			}

			Node *curr = prev;

			// iterating over the list
			while (curr->next != NULL && curr->next->data <= data){
				if (curr->next->data == data){
					// value exists in list, unlock and return false
					pthread_mutex_unlock(&curr->node_mutex);
					return false;
				}
				prev = curr;
				curr = curr->next;
				pthread_mutex_lock(&curr->node_mutex);
				pthread_mutex_unlock(&prev->node_mutex);
			}

			if (curr != head && curr->data == data){
				// value exists in list, unlock and return false
				pthread_mutex_unlock(&curr->node_mutex);
				return false;
			}

			// adding new node
			Node *newNode = new Node(data);
			newNode->next = curr->next;
			curr->next = newNode;
			pthread_mutex_lock(&list_mutex);
			size++;
			pthread_mutex_unlock(&list_mutex);
			__add_hook();
			pthread_mutex_unlock(&curr->node_mutex);
			return true;
		}

		/**
		* Remove the node that its data equals to @param value
		* @param value the data to lookup a node that has the same data to be removed
		* @return true if a matched node was found and removed and false otherwise
		*/
		bool remove(const T& value) {
			// lock dummy node
			Node *prev = head;
			pthread_mutex_lock(&prev->node_mutex);

			if (prev->next == NULL) {
				// in case list is empty
				pthread_mutex_unlock(&prev->node_mutex);
				return false;
			}
			// lock 1st node
			Node *curr = prev->next;
			pthread_mutex_lock(&curr->node_mutex);

			// iterating over the list
			while (curr->next != NULL && curr->data <= value) {
				if (curr->data == value) {
					// removing node from list
					prev->next = curr->next;
					pthread_mutex_lock(&list_mutex);
					size--;
					pthread_mutex_unlock(&list_mutex);
					__remove_hook();
					// unlock and deallocate mem
					pthread_mutex_unlock(&curr->node_mutex);
					pthread_mutex_destroy(&curr->node_mutex);
					delete curr;
					pthread_mutex_unlock(&prev->node_mutex);
					return true;
				}

				pthread_mutex_unlock(&prev->node_mutex);
				prev = curr;
				curr = curr->next;
				pthread_mutex_lock(&curr->node_mutex);
			}

			if (curr->data == value) {
				// removing node from list
				prev->next = curr->next;
				pthread_mutex_lock(&list_mutex);
				size--;
				pthread_mutex_unlock(&list_mutex);
				__remove_hook();
				// unlock and deallocate mem
				pthread_mutex_unlock(&curr->node_mutex);
				pthread_mutex_destroy(&curr->node_mutex);
				delete curr;
				pthread_mutex_unlock(&prev->node_mutex);
				return true;
			}

			// value not found, unlock and return false
			pthread_mutex_unlock(&curr->node_mutex);
			pthread_mutex_unlock(&prev->node_mutex);
			return false;
		}

		/**
		* Returns the current size of the list
		* @return the list size
		*/
		unsigned int getSize() {
			return size;
		}

		// Don't remove
		void print() {
			pthread_mutex_lock(&list_mutex);
			Node* temp = head->next;
			if (temp == NULL) {
				cout << "";
			} else if (temp->next == NULL) {
				cout << temp->data;
			} else {
				while (temp != NULL) {
					cout << right << setw(3) << temp->data;
					temp = temp->next;
					cout << " ";
				}
			}
			cout << endl;
			pthread_mutex_unlock(&list_mutex);
		}

		// Don't remove
		virtual void __add_hook() {}
		// Don't remove
		virtual void __remove_hook() {}

	private:
		Node* head;
		unsigned int size;
		pthread_mutex_t list_mutex;
};

#endif //THREAD_SAFE_LIST_H_