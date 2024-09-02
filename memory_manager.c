
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>






// TODO 1: Define your input parameter (pid - int) here
// Then use module_param to pass them from insmod command line. (--Assignment 2)


static int pid = 0;
module_param(pid, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(pid, "Process ID");




struct task_struct *task = NULL;
// Initialize memory statistics variables
unsigned long total_rss = 0;
unsigned long total_swap = 0;
unsigned long total_wss = 0;




static void parse_vma(void)
{
    struct vm_area_struct *vma = NULL;
    struct mm_struct *mm = NULL;
    struct vma_iterator vmi;
   
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;




    if(pid > 0) {
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if(task && task->mm) {
            mm = task->mm;            
            mmap_read_lock(mm);
            VMA_ITERATOR(vmi,mm, 0);
            for_each_vma(vmi, vma) {
                unsigned long address;
                for (address = vma->vm_start; address < vma->vm_end; address += PAGE_SIZE) {
                    pgd = pgd_offset(mm, address);
                    if (!pgd_present(*pgd)) continue;


                    p4d = p4d_offset(pgd, address);
                    if (!p4d_present(*p4d)) continue;


                    pud = pud_offset(p4d, address);
                    if (!pud_present(*pud)) continue;


                    pmd = pmd_offset(pud, address);
                    if (!pmd_present(*pmd)) continue;


                    pte = pte_offset_kernel(pmd, address);
                    if (!pte_present(*pte)) continue;
                   
                    if(pte_present(*pte)) {
                        // Page is in memory
                        total_rss++;
                        if (pte_young(*pte)) {
                            total_wss++;
                            pte_t old_pte = pte_mkold(*pte); 
                            // Clear the accessed bit
                            set_pte_at(mm, address, pte, old_pte);
                        }
                        // TODO: Add logic to handle pages in swap if needed
                        //pte_unmap(pte); // Unmap after using pte
                    }
                    else if (!pte_none(*pte)) {
    				total_swap++;
                    }
                    pte_unmap(pte);
                }
            }
            //up_read(&mm->mmap_base); // Release the read lock for the memory map of the task
            mmap_read_unlock(mm);
        }
    }
}




unsigned long timer_interval_ns = 10e9; // 10 sec timer
static struct hrtimer hr_timer;


enum hrtimer_restart timer_callback( struct hrtimer *timer_for_restart )
{
    ktime_t currtime , interval;
    currtime  = ktime_get();
    interval = ktime_set(0,timer_interval_ns);
    hrtimer_forward(timer_for_restart, currtime , interval);
    total_rss = 0;
    total_swap = 0;
    total_wss = 0;
    parse_vma();


    printk("[PID-%i]:[RSS:%lu KB] [Swap:%lu KB] [WSS:%lu KB]\n", pid, total_rss * 4, total_swap * 4, total_wss * 4);


    return HRTIMER_RESTART;
}




int memory_init(void){
    printk("CSE330 Project 2 Kernel Module Inserted\n");
    ktime_t ktime;
    ktime = ktime_set( 0, timer_interval_ns );
    hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
    hr_timer.function = &timer_callback;
    hrtimer_start( &hr_timer, ktime, HRTIMER_MODE_REL );
    return 0;
}




void memory_cleanup(void){
    int ret;
    ret = hrtimer_cancel( &hr_timer );
    if (ret) printk("HR Timer cancelled ...\n");
    printk("CSE330 Project 2 Kernel Module Removed\n");
}




module_init(memory_init);
module_exit(memory_cleanup);


MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wilson Kelly");
MODULE_DESCRIPTION("CSE330 Project 2 Memory Management\n");




//
