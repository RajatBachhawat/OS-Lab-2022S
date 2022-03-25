#include "avl.h"
#include <iostream>
using namespace std;

const int AVL_MEM = 500000;

template<typename T>
ostream& operator<<(ostream& op, const pair<T,T>p)
{
    op<<p.first<<","<<p.second;
    return op;
}

template <typename T>
int AVLTree<T>::height(Node<T> *head){
    if(head==nullptr) return 0;
    return head->height;
}

template <typename T>
Node<T> *AVLTree<T>::rightRotation(Node<T> *head){
    Node<T> * newhead = head->left;
    head->left = newhead->right;
    newhead->right = head;
    head->height = 1+max(height(head->left), height(head->right));
    newhead->height = 1+max(height(newhead->left), height(newhead->right));
    return newhead;
}

template <typename T>
Node<T> *AVLTree<T>::leftRotation(Node<T> *head){
    Node<T> * newhead = head->right;
    head->right = newhead->left;
    newhead->left = head;
    head->height = 1+max(height(head->left), height(head->right));
    newhead->height = 1+max(height(newhead->left), height(newhead->right));
    return newhead;
}

template <typename T>
void AVLTree<T>::inorderUtil(Node<T> *head){
    
    if(head==nullptr) return ;
    inorderUtil(head->left);
    cout<<head->data<<" ";
    inorderUtil(head->right);
}

template <typename T>
Node<T> *AVLTree<T>::insertUtil(Node<T> *head, T x){
    if(head == nullptr){
        numNodes+=1;
        Node<T> *temp = ptr;
        temp->data = x;
        temp->height = 1;
        temp->left = nullptr;
        temp->right = nullptr;
        ptr++;
        return temp;
    }
    if(x < head->data) head->left = insertUtil(head->left, x);
    else if(x > head->data) head->right = insertUtil(head->right, x);
    head->height = 1 + max(height(head->left), height(head->right));
    int bal = height(head->left) - height(head->right);
    if(bal>1){
        if(x < head->left->data){
            return rightRotation(head);
        }else{
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    }else if(bal<-1){
        if(x > head->right->data){
            return leftRotation(head);
        }else{
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }
    return head;
}

template <typename T>
Node<T> *AVLTree<T>::removeUtil(Node<T> *head, T x){
    if(head == nullptr) return nullptr;
    if(x < head->data){
        head->left = removeUtil(head->left, x);
    }else if(x > head->data){
        head->right = removeUtil(head->right, x);
    }else{
        Node<T> * r = head->right;
        if(head->right == nullptr){
            Node<T> * l = head->left;
            head = l;
        }else if(head->left == nullptr){
            head = r;
        }else{
            while(r->left!=nullptr) r = r->left;
            head->data = r->data;
            head->right = removeUtil(head->right, r->data);
        }
    }
    if(head == nullptr) return head;
    head->height = 1 + max(height(head->left), height(head->right));
    int bal = height(head->left) - height(head->right);
    if(bal>1){
        if(height(head->left) >= height(head->right)){
            return rightRotation(head);
        }else{
            head->left = leftRotation(head->left);
            return rightRotation(head);
        }
    }else if(bal < -1){
        if(height(head->right) >= height(head->left)){
            return leftRotation(head);
        }else{
            head->right = rightRotation(head->right);
            return leftRotation(head);
        }
    }
    return head;
}

template <typename T>
Node<T> *AVLTree<T>::searchUtil(Node<T> *head, T x){
    if(head == nullptr) return nullptr;
    T k = head->data;
    if(k == x) return head;
    if(k > x) return searchUtil(head->left, x);
    if(k < x) return searchUtil(head->right, x);
}

template <typename T>
Node<T> *AVLTree<T>::lower_bound_util(Node<T> *head, T x){
    if(head == nullptr)
        return nullptr;
    if(head->data == x)
        return head;
    if(head->data < x)
        return lower_bound_util(head->right,x);
    if(head->data > x)
    {
        Node<T> *val = lower_bound_util(head->left,x);
        if(val == nullptr)
            return head;
        else 
            return val;
    }
    return head;
}

template <typename T>
Node<T> *AVLTree<T>::endUtil(Node<T> *head){
    if(head->right == nullptr)
        return head;
    return endUtil(head->right);
}

template <typename T>
void AVLTree<T>::secondLastUtil(Node<T> *head, T *secondLastElement){
    if(numNodes < 2){
        return;
    }
    if(head->right == nullptr){
        if(head->left != nullptr){
            *secondLastElement = max(*secondLastElement, endUtil(head->left)->data);
        }
        return;
    }
    *secondLastElement = max(*secondLastElement, head->data);
    return secondLastUtil(head->right, secondLastElement);
}