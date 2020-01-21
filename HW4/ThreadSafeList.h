#ifndef THREAD_SAFE_LIST_H_
#define THREAD_SAFE_LIST_H_

#include <pthread.h>
#include <iostream>
#include <iomanip> // std::setw

using namespace std;

const unsigned int INITIAL_LIST_SIZE = 0;

template <typename T>
class List 
{
  public:
    /**
     * Constructor
     */
    List() : head(nullptr), size(INITIAL_LIST_SIZE) 
    {
      pthread_mutex_init(&list_mutex);
    }

    /**
     * Destructor
     */  
    ~List() 
    {
      pthread_mutex_destroy(&list_mutex);
    }

    class Node
    {
      public:
        T data;
        Node *next;
        pthread_mutex_t node_mutex;

        Node(T data) : data(data), next(nullptr) 
        {
          pthread_mutex_init(&node_mutex);
        }

    };

    /**
     * Insert new node to list while keeping the list ordered in an ascending order
     * If there is already a node has the same data as @param data then return false (without adding it again)
     * @param data the new data to be added to the list
     * @return true if a new node was added and false otherwise
     */
    bool insert(const T& data) 
    {
      Node* prev, curr;

      prev = head;
      if (prev == nullptr)
      {
        // If our list is empty, the new inserted node becomes its head
        Node* newNode = new Node(data);
        head = newNode;
        pthread_mutex_lock(&list_mutex);
        size++;
        pthread_mutex_unlock(&list_mutex);
        __add_hook();
        return true;
      }

      curr = prev->next;
      if (curr == nullptr)
      {          
        if (prev->data == data)
        {
          pthread_mutex_unlock(&prev->node_mutex);
          return false;
        }
        else 
        {
          Node* newNode = new Node(data);
          if (prev->data > data)
          {
            Node* temp = prev;
            prev = newNode;
            newNode->next = temp;
          }
          else
          {
            prev->next = newNode;              
          }
          
          pthread_mutex_lock(&list_mutex);
          size++;
          pthread_mutex_unlock(&list_mutex);
          __add_hook();
          pthread_mutex_unlock(&prev->node_mutex);
          return true;
        }
      }
      pthread_mutex_lock(&curr->node_mutex);

      // Iterating over the list
      while (curr->data <= data)
      {
        if (curr->data == data)
        {
          pthread_mutex_unlock(&curr->node_mutex);
          pthread_mutex_unlock(&prev->node_mutex);
          return false;
        }
        
        pthread_mutex_unlock(&prev->node_mutex);
        prev = curr;
        curr = curr->next;
        pthread_mutex_lock(&curr->node_mutex);
      }

      Node* newNode = new Node(data);
      prev->next = newNode;
      pthread_mutex_lock(&list_mutex);
      size++;
      pthread_mutex_unlock(&list_mutex);
      __add_hook();
      pthread_mutex_unlock(&curr->node_mutex);
      pthread_mutex_unlock(&prev->node_mutex);
      return true;
    }

    /**
     * Remove the node that its data equals to @param value
     * @param value the data to lookup a node that has the same data to be removed
     * @return true if a matched node was found and removed and false otherwise
     */
    bool remove(const T& value) 
    {
      T nodeData = value;
      Node* prev, curr;

      prev = head;
      if (prev == nullptr)
      {
        // If our list is empty, there is no need to iterate over it
        return false;
      }
      pthread_mutex_lock(&prev->node_mutex);
      
      curr = prev->next;        
      if (curr == nullptr)
      {
        // If our list contains only one element, there is no need to iterate over it
        if (prev->data == value)
        {
          prev = curr;
          pthread_mutex_lock(&list_mutex);
          size--;
          pthread_mutex_unlock(&list_mutex);
          __remove_hook();
          pthread_mutex_unlock(&prev->node_mutex);
          return true;
        }
        else
        {
          pthread_mutex_unlock(&prev->node_mutex);
          return false;
        }
      }
      pthread_mutex_lock(&curr->node_mutex);

      // Iterating over the list
      while (curr->data <= value)
      {
        if (curr->data == value)
        {
          prev->next = curr->next;
          pthread_mutex_lock(&list_mutex);
          size--;
          pthread_mutex_unlock(&list_mutex);
          __remove_hook();
          pthread_mutex_unlock(&curr->node_mutex);
          pthread_mutex_unlock(&prev->node_mutex);
          return true;
        }
        
        pthread_mutex_unlock(&prev->node_mutex);
        prev = curr;
        curr = curr->next;
        pthread_mutex_lock(&curr->node_mutex);
      }
      
      pthread_mutex_unlock(&curr->node_mutex);
      pthread_mutex_unlock(&prev->node_mutex);
      return false;
    }

    /**
     * Returns the current size of the list
     * @return the list size
     */
    unsigned int getSize() 
    {
      return size;
    }

    // Don't remove
    void print() 
    {
      pthread_mutex_lock(&list_mutex);
      Node* temp = head;
      if (temp == NULL)
      {
        cout << "";
      }
      else if (temp->next == NULL)
      {
        cout << temp->data;
      }
      else
      {
        while (temp != NULL)
        {
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