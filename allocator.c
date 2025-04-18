#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_COMMAND_LENGTH 64
#define MAX_PROCESSES 100

// Structure to represent a memory block (allocated or free)
typedef struct MemoryBlock
{
    uintptr_t start_address;
    size_t size;
    int allocated;       // 1 if allocated, 0 if free
    char process_id[10]; // Identifier for allocated blocks
    struct MemoryBlock *next;
} MemoryBlock;

uintptr_t memory_start;
size_t total_memory_size;
MemoryBlock *memory_list = NULL;

// Function to initialize the memory manager
void initialize_memory(size_t size)
{
    total_memory_size = size;
    memory_start = (uintptr_t)malloc(total_memory_size);
    if (!memory_start)
    {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    memory_list = (MemoryBlock *)malloc(sizeof(MemoryBlock));
    if (memory_list == NULL)
    {
        perror("Failed to allocate memory block metadata");
        free((void *)memory_start);
        exit(EXIT_FAILURE);
    }

    memory_list->start_address = memory_start;
    memory_list->size = total_memory_size;
    memory_list->allocated = 0;
    strcpy(memory_list->process_id, "");
    memory_list->next = NULL;
}

// Function to display the current memory status
void display_memory_status()
{
    printf("Memory Status:\n");
    MemoryBlock *current = memory_list;
    while (current != NULL)
    {
        printf("Address [%p - %p] Size: %zu bytes, Status: %s",
               (void *)current->start_address,
               (void *)(current->start_address + current->size - 1),
               current->size,
               current->allocated ? current->process_id : "Free");
        if (current->next != NULL)
        {
            printf(" -> ");
        }
        printf("\n");
        current = current->next;
    }
    printf("Total memory: %zu bytes\n", total_memory_size);
}

// Function to find the first-fit free block
MemoryBlock *find_first_fit(size_t size)
{
    MemoryBlock *current = memory_list;
    while (current != NULL)
    {
        if (!current->allocated && current->size >= size)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Function to find the best-fit free block
MemoryBlock *find_best_fit(size_t size)
{
    MemoryBlock *best_fit = NULL;
    MemoryBlock *current = memory_list;
    size_t min_diff = SIZE_MAX;

    while (current != NULL)
    {
        if (!current->allocated && current->size >= size)
        {
            size_t diff = current->size - size;
            if (diff < min_diff)
            {
                min_diff = diff;
                best_fit = current;
            }
        }
        current = current->next;
    }
    return best_fit;
}

// Function to find the worst-fit free block
MemoryBlock *find_worst_fit(size_t size)
{
    MemoryBlock *worst_fit = NULL;
    MemoryBlock *current = memory_list;
    size_t max_size = 0;

    while (current != NULL)
    {
        if (!current->allocated && current->size >= size)
        {
            if (current->size > max_size)
            {
                max_size = current->size;
                worst_fit = current;
            }
        }
        current = current->next;
    }
    return worst_fit;
}

// Function to allocate memory
void allocate_memory(char *process_id, size_t size, char strategy)
{
    MemoryBlock *free_block = NULL;

    if (strategy == 'F')
    {
        free_block = find_first_fit(size);
    }
    else if (strategy == 'B')
    {
        free_block = find_best_fit(size);
    }
    else if (strategy == 'W')
    {
        free_block = find_worst_fit(size);
    }
    else
    {
        printf("Error: Invalid allocation strategy '%c'\n", strategy);
        return;
    }

    if (free_block == NULL)
    {
        printf("Error: Not enough memory to allocate %zu bytes for process %s\n", size, process_id);
        return;
    }

    // If the free block is larger than needed, split it
    if (free_block->size > size)
    {
        MemoryBlock *new_block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
        if (new_block == NULL)
        {
            perror("Failed to allocate memory block metadata");
            return;
        }
        new_block->start_address = free_block->start_address + size;
        new_block->size = free_block->size - size;
        new_block->allocated = 0;
        strcpy(new_block->process_id, "");
        new_block->next = free_block->next;
        free_block->next = new_block;
    }

    free_block->allocated = 1;
    free_block->size = size;
    strcpy(free_block->process_id, process_id);
    printf("Allocated %zu bytes to process %s at address %p\n", size, process_id, (void *)free_block->start_address);
}

// Function to release memory
void release_memory(char *process_id)
{
    MemoryBlock *current = memory_list;
    MemoryBlock *prev = NULL;

    while (current != NULL)
    {
        if (current->allocated && strcmp(current->process_id, process_id) == 0)
        {
            current->allocated = 0;
            strcpy(current->process_id, "");
            printf("Released memory allocated to process %s at address %p, size %zu bytes\n",
                   process_id, (void *)current->start_address, current->size);

            // Try to merge with adjacent free blocks
            if (prev != NULL && !prev->allocated)
            {
                prev->size += current->size;
                prev->next = current->next;
                free(current);
                current = prev; // Continue checking from the merged block
            }
            MemoryBlock *next = current->next;
            if (next != NULL && !next->allocated)
            {
                current->size += next->size;
                current->next = next->next;
                free(next);
            }
            return;
        }
        prev = current;
        current = current->next;
    }

    printf("Error: Process %s not found or has no allocated memory.\n", process_id);
}

// Function to compact memory
void compact_memory()
{
    printf("Compacting memory...\n");
    MemoryBlock *current = memory_list;
    uintptr_t current_address = memory_start;
    MemoryBlock *new_list = NULL;
    MemoryBlock *tail = NULL;

    // First pass: Collect all allocated blocks and move them to the beginning
    while (current != NULL)
    {
        if (current->allocated)
        {
            // Create a new block in the compacted memory
            MemoryBlock *new_block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
            if (!new_block)
            {
                perror("Failed to allocate memory block metadata during compaction");
                // Attempt to clean up (might leave memory in inconsistent state)
                MemoryBlock *temp = new_list;
                while (temp)
                {
                    MemoryBlock *next_temp = temp->next;
                    free(temp);
                    temp = next_temp;
                }
                return;
            }
            new_block->start_address = current_address;
            new_block->size = current->size;
            new_block->allocated = 1;
            strcpy(new_block->process_id, current->process_id);
            new_block->next = NULL;

            if (tail == NULL)
            {
                new_list = tail = new_block;
            }
            else
            {
                tail->next = new_block;
                tail = new_block;
            }
            current_address += current->size;
        }
        current = current->next;
    }

    // Free the old memory block list
    current = memory_list;
    while (current != NULL)
    {
        MemoryBlock *next = current->next;
        free(current);
        current = next;
    }
    memory_list = new_list;

    // Add a single free block at the end if there's any remaining space
    if (current_address < memory_start + total_memory_size)
    {
        MemoryBlock *free_block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
        if (!free_block)
        {
            perror("Failed to allocate free block metadata after compaction");
            // Handle error (memory_list is already updated with allocated blocks)
            return;
        }
        free_block->start_address = current_address;
        free_block->size = memory_start + total_memory_size - current_address;
        free_block->allocated = 0;
        strcpy(free_block->process_id, "");
        free_block->next = NULL;

        if (tail == NULL)
        {
            memory_list = free_block;
        }
        else
        {
            tail->next = free_block;
        }
    }
    else if (tail != NULL)
    {
        tail->next = NULL; // Ensure the last allocated block points to NULL
    }
    printf("Memory compaction complete.\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <memory_size_in_bytes>\n", argv[0]);
        return 1;
    }

    size_t memory_size = strtoul(argv[1], NULL, 10);
    if (memory_size == 0)
    {
        fprintf(stderr, "Error: Invalid memory size.\n");
        return 1;
    }

    initialize_memory(memory_size);

    char command[MAX_COMMAND_LENGTH];

    while (1)
    {
        printf("allocator> ");
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("\nExiting.\n");
            break;
        }

        // Remove trailing newline character
        command[strcspn(command, "\n")] = 0;

        char action[10];
        if (sscanf(command, "%s", action) == 1)
        {
            if (strcmp(action, "RQ") == 0)
            {
                char process_id[10];
                size_t size;
                char strategy[2];
                if (sscanf(command, "%s %s %zu %s", action, process_id, &size, strategy) == 4)
                {
                    allocate_memory(process_id, size, strategy[0]);
                }
                else
                {
                    printf("Usage: RQ <process_id> <size> <F|B|W>\n");
                }
            }
            else if (strcmp(action, "RL") == 0)
            {
                char process_id[10];
                if (sscanf(command, "%s %s", action, process_id) == 2)
                {
                    release_memory(process_id);
                }
                else
                {
                    printf("Usage: RL <process_id>\n");
                }
            }
            else if (strcmp(action, "C") == 0)
            {
                compact_memory();
            }
            else if (strcmp(action, "STAT") == 0)
            {
                display_memory_status();
            }
            else if (strcmp(action, "X") == 0)
            {
                printf("Exiting.\n");
                break;
            }
            else
            {
                printf("Error: Unknown command '%s'\n", action);
            }
        }
    }

    // Clean up memory before exiting
    MemoryBlock *current = memory_list;
    while (current != NULL)
    {
        MemoryBlock *next = current->next;
        free(current);
        current = next;
    }
    free((void *)memory_start);

    system("pause");

    return 0;
}
