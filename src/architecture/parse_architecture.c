#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <stdint.h>
#include <stdbool.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <inttypes.h>

#include "../../include/x86_energy.h"

static int read_file_long(char * file, long int* result)
{
    char buffer[2048];
    int fd=open(file, O_RDONLY);
    if (fd < 0 ) return 1;
    int read_bytes=read(fd,buffer,2048);
    close(fd);
    if (read_bytes < 0 )
    {
        return 1;
    }
    char * endptr;
    *result=strtoll(buffer,&endptr,10);
    return 0;
}

static int read_file_long_mask(char * file, long int** result,int* length)
{
    char buffer[2048];
    int fd=open(file, O_RDONLY);
    if (fd < 0 ) return 1;
    int read_bytes=read(fd,buffer,2048);
    close(fd);
    /* would need larger buffer */
    if (read_bytes == 2048)
    {
        return 1;
    }
    if (read_bytes < 0 )
    {
        return 1;
    }
    int end_of_text = read_bytes-1;
    *result = NULL;
    *length=0;
    long int nr_processed=0;
    for (int position_in_text = end_of_text;position_in_text>=0;position_in_text--)
    {
        /* end string early */
        buffer[position_in_text + 1 ] = '\0';
        /* now read that ull */
        char * endptr;
        long int chunk = strtol(&buffer[position_in_text],&endptr,16);

        /* sometimes there are commas */
        if (endptr==&buffer[position_in_text]){
            continue;
        }

        for (int i=0;i<4;i++)
        {
            if ((chunk >> (3-i) & 1) )
            {
                long int * tmp = realloc(*result,((*length)+1)*sizeof(**result));
                if (!tmp)
                {
                    free(*result);
                    *result=NULL;
                    *length=0;
                    return 1;
                }
                *result=tmp;
                tmp[*length]=nr_processed*4+i;
                (*length)++;
            }
        }
        nr_processed++;
    }
    return 0;
}

/*checks for /sys/devices/system/cpu/node/node<n> */
static int get_nodes( char * sysfs, x86_energy_architecture_node_t ** nodes, int * nr_nodes)
{
    *nodes=NULL;
    *nr_nodes=0;
    /* TODO get filenames */
    char fs[256];
    int ret=snprintf(fs,256,"%s/devices/system/node",sysfs);

    if ( ret<0 || ret>256)
        return 1;

    DIR *d;
    struct dirent *dir;
    d = opendir(fs);
    char** filenames=NULL;
    int nr_filenames=0;
    if (d) {
      while ((dir = readdir(d)) != NULL) {
          if (strncmp(dir->d_name,"node",4) == 0 && dir->d_type == DT_DIR && dir->d_name[4]>='0' && dir->d_name[4]<='9')
          {
              char ** tmp=realloc(filenames,sizeof(char*)*nr_filenames+1);
              if ( tmp == NULL)
              {
                  for (int i=0;i<nr_filenames;i++)
                  {
                      free(filenames[i]);
                  }
                  if ( filenames )
                      free(filenames);
                  closedir(d);
                  return 1;
              }
              filenames=tmp;
              filenames[nr_filenames]=strdup(dir->d_name);
              if (filenames[nr_filenames] == NULL)
              {
                  for (int i=0;i<nr_filenames;i++)
                  {
                      free(filenames[i]);
                  }
                  if ( filenames )
                      free(filenames);
                  closedir(d);
                  return 1;
              }
              nr_filenames++;
          }
      }
      closedir(d);
    }

    for (int i=0;i<nr_filenames;i++)
    {
        char * filename=filenames[i];
        char * endptr;
        /* get %d */
        long int node = strtol(filename+4,&endptr,10);
        x86_energy_architecture_node_t* tmp=realloc(*nodes,(*nr_nodes+1)*sizeof(**nodes));
        if (tmp == NULL)
        {
            for (int i=0;i<*nr_nodes;i++)
                free((*nodes)[i].name);
            free(*nodes);
            *nodes=NULL;
            *nr_nodes=0;
            return -1;
        }
        *nodes=tmp;
        (*nodes)[*nr_nodes].granularity=X86_ENERGY_GRANULARITY_DIE;
        (*nodes)[*nr_nodes].id=node;
        (*nodes)[*nr_nodes].children=NULL;
        (*nodes)[*nr_nodes].nr_children=0;
        char buffer[16];
        sprintf(buffer,"%ld",node);
        (*nodes)[*nr_nodes].name=strdup(buffer);
        (*nr_nodes)++;
        free(filenames[i]);
        filenames[i]=NULL;
    }
    free(filenames);
    return 0;
}

static int insert_new_child(x86_energy_architecture_node_t* parent_node,int granularity,long int id, const char * name)
{
    char* new_name = strdup(name);
    if ( new_name == NULL)
    {
        return 1;
    }
    x86_energy_architecture_node_t* tmp = realloc(parent_node->children,(parent_node->nr_children+1)*sizeof(x86_energy_architecture_node_t));
    if (tmp == NULL)
    {
        return 1;
    }
    parent_node->children=tmp;
    parent_node->children[parent_node->nr_children].id=id;
    parent_node->children[parent_node->nr_children].name=new_name;
    parent_node->children[parent_node->nr_children].granularity = granularity;
    parent_node->children[parent_node->nr_children].children=NULL;
    parent_node->children[parent_node->nr_children].nr_children=0;
    parent_node->nr_children++;
    return 0;
}

static x86_energy_architecture_node_t* find_child(x86_energy_architecture_node_t* parent_node,long id)
{
    for (int i = 0; i < parent_node->nr_children; i++)
    {
        if (id == parent_node->children[i].id)
        {
            return &parent_node->children[i];
        }
    }
    return NULL;
}

static int add_cpu_and_core_to_node(const char* sysfs_path, x86_energy_architecture_node_t* parent_node, long int cpu)
{
    char buffer[512];
    snprintf(buffer,512,"%s/devices/system/cpu/cpu%ld/topology/core_id",sysfs_path,cpu);
    /* TODO test */
    long int core;
    if (read_file_long(buffer,&core))
    {
        return 1;
    }
    x86_energy_architecture_node_t * core_node = find_child(parent_node,core);
    if ( core_node == NULL )
    {
        char name[256];
        sprintf(name,"Core %ld",core);
        if (insert_new_child(parent_node,X86_ENERGY_GRANULARITY_CORE,core,name) )
            return 1;
        core_node = &parent_node->children[parent_node->nr_children-1];
    }
    x86_energy_architecture_node_t * cpu_node = find_child(core_node,cpu);
    if ( cpu_node == NULL )
    {
        char name[256];
        sprintf(name,"CPU %ld",cpu);
        if (insert_new_child(core_node,X86_ENERGY_GRANULARITY_THREAD,cpu,name) )
            return 1;
    }
    return 0;
}

static bool find_cpu(x86_energy_architecture_node_t * node,int cpu)
{
    if (node->granularity==X86_ENERGY_GRANULARITY_THREAD)
    {
        if (node->id == cpu)
            return true;
        else
            return false;
    }
    for (int i=0;i<node->nr_children;i++)
        if (find_cpu(&node->children[i],cpu))
            return true;
    return false;
}

static int add_package(x86_energy_architecture_node_t* sys_node,long int package_id,x86_energy_architecture_node_t **package_node)
{
    *package_node=find_child(sys_node,package_id);
    if (*package_node == NULL)
    {
        char package_name[512];
        sprintf(package_name,"Processor %ld",package_id);
        if (insert_new_child(sys_node,X86_ENERGY_GRANULARITY_SOCKET,package_id,package_name))
        {
            return 1;
        }
        *package_node=&sys_node->children[sys_node->nr_children-1];
    }
    return 0;
}

static int add_node_to_package(x86_energy_architecture_node_t* package,x86_energy_architecture_node_t** node)
{
    x86_energy_architecture_node_t* found_node = find_child(package,(*node)->id );
    if (found_node == NULL)
    {
        x86_energy_architecture_node_t* tmp = realloc(package->children,
                sizeof(x86_energy_architecture_node_t) * (package->nr_children + 1));
        if (tmp == NULL)
        {
            return 1;
        }
        package->children = tmp;
        package->children[package->nr_children] = **node;
        (*node)->name=NULL;
        found_node=&package->children[package->nr_children];
        package->nr_children++;
    }
    *node=found_node;
    return 0;
}

static int process_node(const char* sysfs_path,x86_energy_architecture_node_t * sys_node,x86_energy_architecture_node_t* node)
{
    long int * cpus;
    int nr_cpus;
    char filename[2048];
    sprintf(filename, "%s/devices/system/node/node%" PRId32 "/cpumap", sysfs_path,node->id);
    if (read_file_long_mask(filename, &cpus, &nr_cpus))
    {
        return 1;
    }
    for (int current_cpu = 0; current_cpu < nr_cpus; current_cpu++)
    {
        long int cpu=cpus[current_cpu];
        /* try to find cpu */
        if ( find_cpu(node, cpu) )
            continue;
        long int package_id;
        sprintf(filename,
                "%s/devices/system/cpu/cpu%ld/topology/physical_package_id",
                sysfs_path, cpu);
        if (read_file_long(filename, &package_id))
        {
            free(cpus);
            return 1;
        }
        x86_energy_architecture_node_t * package = NULL;
        if ( add_package(sys_node,package_id,&package))
        {
            free(cpus);
            return 1;
        }

        if (add_node_to_package(package,&node))
        {
            free(cpus);
            return 1;
        }

        /* now the module */

        x86_energy_architecture_node_t* new_parent = node;

        int nr_shared_cpus_l2;
        long int *shared_cpus_l2;
        sprintf(filename,
                "%s/devices/system/cpu/cpu%ld/cache/index2/shared_cpu_map",
                sysfs_path, cpu);
        if (read_file_long_mask(filename, &shared_cpus_l2, &nr_shared_cpus_l2))
        {
            free(cpus);
            return 1;
        }

        int nr_shared_cpus_l1;
        long int *shared_cpus_l1;
        sprintf(filename,
                "%s/devices/system/cpu/cpu%ld/cache/index1/shared_cpu_map",
                sysfs_path, cpu);
        if (read_file_long_mask(filename, &shared_cpus_l1, &nr_shared_cpus_l1))
        {
            free(cpus);
            return 1;
        }
        free(shared_cpus_l1);
        if (nr_shared_cpus_l2 > nr_shared_cpus_l1)
        {
            char buffer[256];
            sprintf(buffer, "module %d", (int) node->nr_children);
            if (insert_new_child(node,X86_ENERGY_GRANULARITY_MODULE,node->nr_children,buffer))
            {
                free(shared_cpus_l2);
                free(cpus);
                return 1;
            }
            new_parent=&(node->children[node->nr_children-1]);
            for (int j=0;j<nr_shared_cpus_l2;j++)
            {
                add_cpu_and_core_to_node(sysfs_path,new_parent,shared_cpus_l2[j]);
            }
        }
        else
            add_cpu_and_core_to_node(sysfs_path,new_parent,cpu);
        free(shared_cpus_l2);
    }
    free(cpus);
    return 0;
}

void x86_energy_print(x86_energy_architecture_node_t * node,int level)
{
    for (int i=0;i<level;i++)
        printf("  ");
    switch (node->granularity)
    {
    case X86_ENERGY_GRANULARITY_THREAD:
        printf("CPU id: %" PRId32 " %s\n",node->id, node->name);
        return;
    case X86_ENERGY_GRANULARITY_CORE:
        printf("Core: %" PRId32 " %s\n",node->id,node->name);
        break;
    case X86_ENERGY_GRANULARITY_MODULE:
        printf("Module: %" PRId32 " %s\n",node->id,node->name);
        break;
    case X86_ENERGY_GRANULARITY_DIE:
        printf("Die: %" PRId32 " %s\n",node->id,node->name);
        break;
    case X86_ENERGY_GRANULARITY_SOCKET:
        printf("Socket: %" PRId32 " %s\n",node->id,node->name);
        break;
    case X86_ENERGY_GRANULARITY_SYSTEM:
        printf("System: %" PRId32 " %s\n",node->id,node->name);
        break;
    default:
        printf("Unknown(%d): %" PRId32 " %s\n",node->granularity,node->id,node->name);
        break;
    }

    for (int i=0;i<node->nr_children;i++)
        x86_energy_print(&node->children[i],level+1);
}

x86_energy_architecture_node_t * x86_energy_init_architecture_nodes( void )
{
    x86_energy_architecture_node_t * sys_node=calloc(1,sizeof(x86_energy_architecture_node_t));
    char hostname[512];
    memset(hostname,0,sizeof(hostname));
    if (gethostname(hostname, 512))
        return NULL;
    sys_node->granularity=X86_ENERGY_GRANULARITY_SYSTEM;
    sys_node->name=strdup(hostname);
    if (sys_node->name == NULL)
    {
        free(sys_node);
        return NULL;
    }
    /* Try open sysfs */
    /* TODO look for sysfs in mnt */
    char * sysfs_path="/sys/";
    x86_energy_architecture_node_t* nodes;
    int nr_nodes=0;
    if (get_nodes(sysfs_path,&nodes,&nr_nodes))
    {
        free(sys_node->name);
        free(sys_node);
        return NULL;
    }
    for (int i=0;i<nr_nodes;i++)
        if (process_node(sysfs_path,sys_node,&nodes[i]))
        {
            free(sys_node->name);
            free(sys_node);
            for (int i=0;i<nr_nodes;i++)
            {
                free(nodes[i].name);
            }
            free(nodes);
            return NULL;
        }
    return sys_node;
}

void x86_energy_free_architecture_nodes( x86_energy_architecture_node_t * root )
{
    for (int i=0;i<root->nr_children;i++)
    {
        x86_energy_free_architecture_nodes( &(root->children[i]) );
    }
    free(root->children);
    free(root->name);

    if (root->granularity == X86_ENERGY_GRANULARITY_SYSTEM)
        free(root);
}

x86_energy_architecture_node_t * x86_energy_find_arch_for_cpu(x86_energy_architecture_node_t * root, enum x86_energy_granularity granularity, int cpu)
{
    if (root->granularity==X86_ENERGY_GRANULARITY_THREAD)
    {
        if (root->id == cpu)
            return root;
        else
            return NULL;
    }
    if (root->granularity==granularity)
    {
        for (int i=0;i<root->nr_children;i++)
        {
            if (find_cpu(&root->children[i],cpu))
                return root;
        }
    }
    else
    {
        for (int i=0;i<root->nr_children;i++)
        {
            x86_energy_architecture_node_t * found = x86_energy_find_arch_for_cpu(&root->children[i],granularity,cpu);
            if (found)
                return found;
        }
    }
    return NULL;
}

int x86_energy_arch_count(x86_energy_architecture_node_t * root, enum x86_energy_granularity granularity)
{
    int sum=0;
    if (root->granularity==granularity)
    {
        return 1;
    }
    else
    {
        for (int i=0;i<root->nr_children;i++)
        {
            sum+=x86_energy_arch_count(&root->children[i],granularity);
        }
    }
    return sum;
}
