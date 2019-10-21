#ifndef __SEM_H_
#define __SEM_H_

struct my_node{
        //data
	//struct proc of lab1 equals to struct task_struct
        struct task_struct * task;
        //pointers, links
        struct my_node * next;
};

struct cs1550_sem{
        int value;
        //queue
        struct my_node * head;
        struct my_node * tail;
};



#endif
