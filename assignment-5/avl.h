#ifndef AVL_H
#define AVL_H

const int AVL_TREE_MEM = 500000;

template<typename T>
class Node {
public:
    T data;
    int height;
    Node *left;
    Node *right;
    Node(T d){
        height = 1;
        data = d;
        left = nullptr;
        right = nullptr;
    }
};

template<typename T>
class AVLTree {
public: 
    char mem[AVL_TREE_MEM];
    Node<T> *root = nullptr;
    Node<T> *ptr = (Node<T> *)mem;
    int numNodes;
    AVLTree()
    {
        ptr = (Node<T> *)mem;
        root = nullptr;
        numNodes = 0;
    }
    void insert(T x){
        root = insertUtil(root, x);
    }
    void remove(T x){
        root = removeUtil(root, x);
    }
    T search(T x){
        return searchUtil(root, x)->data;
    }
    Node<T>* lower_bound(T x){
        return lower_bound_util(root, x);
    }
    void inorder(){    
        inorderUtil(root);
    }
    Node<T> *end(){
        return endUtil(root);
    }
    void secondLast(T *secondLastElement){
        secondLastUtil(root, secondLastElement);
    }
private:
    int height(Node<T> *head);
    Node<T> *rightRotation(Node<T> *head);
    Node<T> *leftRotation(Node<T> *head);
    Node<T> *insertUtil(Node<T> *head, T x);
    Node<T> *removeUtil(Node <T> *head, T x);
    Node<T> *searchUtil(Node <T> *head, T x);
    Node<T> *lower_bound_util(Node <T> *head, T x);
    Node<T> *endUtil(Node<T> *head);
    void inorderUtil(Node<T> *head);
    void secondLastUtil(Node<T> *head, T *secondLastElement);
};

#endif