#ifndef FOOBAR_H
#define FOOBAR_H

extern struct SancusModule foo;
extern struct SancusModule bar;
extern struct SancusModule a;

void do_control_flow_integrity_hack(void);

void SM_ENTRY("a") enter_a(void);
void SM_ENTRY("foo") enter_foo(void);
void SM_ENTRY("bar") enter_bar(void);

void SM_ENTRY("a") set_a_vars(void);
void SM_ENTRY("bar") set_bar_vars(void);
void SM_ENTRY("foo") set_foo_vars(void);

void SM_ENTRY("a") print_a_cur_thr_id(void);
void SM_ENTRY("bar") print_bar_cur_thr_id(void);
void SM_ENTRY("foo") print_foo_cur_thr_id(void);

#endif //FOOBAR_H
