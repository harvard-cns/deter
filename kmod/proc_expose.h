#ifndef _PROC_EXPOSE_H
#define _PROC_EXPOSE_H

#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

struct proc_expose{
	void *data; // pointer to data to expose
	int (*output_func) (void *args, char* buf, size_t len);
	int (*input_func) (void *args, char* buf, size_t len);
	char proc_file_name[64]; // proc file name
	struct file_operations fops; // file_operations struct
};

#define INIT_PROC_EXPOSE(name) \
static ssize_t proc_##name##_read(struct file* file, char __user *buf, size_t len, loff_t *ppos); \
static ssize_t proc_##name##_write(struct file* file, const char *buf, size_t len, loff_t *ppos); \
static struct proc_expose proc_##name##_expose = { \
	.data = NULL, \
	.output_func = NULL, \
	.input_func = NULL, \
	.proc_file_name = #name, \
	.fops = { \
		.owner = THIS_MODULE, \
		.read = proc_##name##_read, \
		.write = proc_##name##_write, \
	}, \
}; \
static ssize_t proc_##name##_read(struct file* file, char __user *buf, size_t len, loff_t *ppos){ \
	char s[32]; \
	int ret; \
	if (!proc_##name##_expose.output_func) \
		return 0; \
	ret = proc_##name##_expose.output_func(proc_##name##_expose.data, s, 32); \
	copy_to_user(buf, s, ret); \
	return ret; \
} \
static ssize_t proc_##name##_write(struct file* file, const char *buf, size_t len, loff_t *ppos){ \
	char s[32]; \
	int ret; \
	if (len > 32){ \
		printk("error: proc_"#name"_expose cannot accept %lu bytes write\n", len); \
		return 0; \
	} \
	if (!proc_##name##_expose.input_func) \
		return 0; \
	copy_from_user(s, buf, len); \
	ret = proc_##name##_expose.input_func(proc_##name##_expose.data, s, len); \
	return ret; \
}

/* Share with user space a memory region starting from `addr`.
 * The starting address is exposed to user via a proc file, with specific name. */
int proc_expose_start(struct proc_expose*);
void proc_expose_stop(struct proc_expose*);

#endif /* _PROC_EXPOSE_H */
