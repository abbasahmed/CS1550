import java.io.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Hashtable;
import java.util.LinkedList;
import java.util.Scanner;

public class Algorithms {

	public final int PAGES = (int) Math.pow(2, 20);

	public int memory_accesses, page_faults, total_writes;

	public Hashtable<Long, PTE> page_table;

	/*
	 * [OPT]imal Algorithm: In case of eviction, choose the page which appears the
	 * farthest next or does not appear at all. In case of any ties, select clean
	 * page, and lowest page number if the tie still remains.
	 */

	public void opt(int frames, File trace_file) throws FileNotFoundException {
		memory_accesses = 0;
		page_faults = 0;
		total_writes = 0;

		int time = 0;
		int count = 0;
		page_table = init_table();
		Hashtable<Long, LinkedList<Integer>> future_table = new Hashtable<Long, LinkedList<Integer>>();
		ArrayList<Long> page_frames = new ArrayList<Long>(frames);

		long[] frames_table = new long[frames];
		Arrays.fill(frames_table, (long) -1);

		// Initialize the table containing the occurrences of each address in the trace
		// file

		for (long i = 0; i < PAGES; i++) {
			future_table.put((long) i, new LinkedList<Integer>());
		}

		Scanner trace_scan;
		trace_scan = new Scanner(trace_file);

		// Update the future table

		while (trace_scan.hasNext()) {
			String trace_line = trace_scan.nextLine();
			String[] trace_args = trace_line.split(" ");
			long address = address(trace_args[1]);
			if (!future_table.containsKey(address))
				future_table.put(address, new LinkedList<Integer>());
			future_table.get(address).add(time);
			time++;
		}
		trace_scan.close();
		trace_scan = new Scanner(trace_file);

		// Read every line of the trace line to start simulating
		while (trace_scan.hasNextLine()) {
			String trace_line = trace_scan.nextLine();
			String[] trace_args = trace_line.split(" ");
			long address = address(trace_args[1]);
			char mode = trace_args[0].charAt(0);

			// Check if address is invalid
			if (address < 0 || address >= PAGES) {
				trace_scan.close();
				return;
			}
			// Remove the current occurrence of the page with the current address
			future_table.get(address).remove();
			if (!page_frames.contains(address)) {
				// PAGE FAULT!
				page_faults++;
				int curr_frame;

				// If there's space, no need to evict.
				if (count < frames) {
					curr_frame = count;
					count++;
				} else {
					// No Space need to evict
					long remove_address = latest_used(future_table, frames_table, page_table);
					curr_frame = page_table.get(remove_address).getFrame();
					total_writes = remove_page(remove_address, page_table, page_frames, total_writes);
				}
				// Replace the frame with new address and add the updated page back to page
				// table
				frames_table[curr_frame] = address;
				add_page(address, page_table, page_frames, mode, curr_frame);
			} else {
				// PAGE HIT!
				if (mode == 's')
					page_table.get(address).setDirtyBit(true);
			}
			memory_accesses++;
		}
		trace_scan.close();
		print_results(memory_accesses, frames, page_faults, total_writes, "opt");
	}

	/*
	 * [FIFO](First In First Out) Algorithm: In case of eviction, choose the page
	 * that is the oldest in the current frames.
	 */

	public void fifo(int frames, File trace_file) throws FileNotFoundException {
		memory_accesses = 0;
		page_faults = 0;
		total_writes = 0;
		page_table = init_table();

		LinkedList<PTE> frames_table = new LinkedList<PTE>();
		ArrayList<Long> page_frames = new ArrayList<Long>(frames);
		int count = 0;
		Scanner trace_scan = new Scanner(trace_file);
		while (trace_scan.hasNextLine()) {
			String trace_string = trace_scan.nextLine();
			String[] trace_args = trace_string.split(" ");
			char mode = trace_args[0].charAt(0);
			long address = address(trace_args[1]);
			// Check if address is valid
			if (address < 0 || address >= PAGES) {
				trace_scan.close();
				return;
			}
			if (!page_frames.contains(address)) {
				// PAGE FAULT!
				page_faults++;
				// There's space to let in
				if (count < frames) {
					count++;
				} else {
					// Need to evict the first page
					total_writes = remove_page(frames_table.getFirst().index, page_table, page_frames, total_writes);
					frames_table.removeFirst();
				}
				// Add the new page to the end of the list
				PTE added_entry = add_page(address, page_table, page_frames, mode, 0);
				frames_table.add(added_entry);
			} else {
				// PAGE HIT!
				if (mode == 's')
					page_table.get(address).setDirtyBit(true);
			}
			memory_accesses++;
		}
		trace_scan.close();
		print_results(memory_accesses, frames, page_faults, total_writes, "fifo");
	}

	/*
	 * [AGING] Algorithm: In case of eviction, choose the page with the lowest age.
	 * Ages get updated periodically.
	 */

	public void aging(int frames, File trace_file, int refresh) throws FileNotFoundException {
		memory_accesses = 0;
		page_faults = 0;
		total_writes = 0;
		page_table = init_table();
		
		ArrayList<Long> page_frames = new ArrayList<Long>(frames);
		int pos = 0;
		int count = -1; // Cycles count at the beginning is set to -1 to avoid memory read for first line
		
		Scanner trace_scan = new Scanner(trace_file);
		while (trace_scan.hasNextLine()) {
			String trace_string = trace_scan.nextLine();
			String[] trace_args = trace_string.split(" ");
			char mode = trace_args[0].charAt(0);
			long address = address(trace_args[1]);
			
			//Check if address is valid
			if (address < 0 || address >= PAGES) {
				trace_scan.close();
				return;
			}
			
			// CPU CYCLES COUNT
			count = count + Integer.parseInt(trace_args[2]) + 1;
			
			// Reset the counters once refresh rate is hit
			if (count >= refresh) {
				int resets = count / refresh;
				count = count % refresh;
				for (int i = 0; i < resets; i++) {
					for (long index : page_frames) {
						PTE update = page_table.get(index);
						update.setAge(next_age(update.getAge(), update.referenced()));
						update.setReferenceBit(false);
						page_table.put(index, update);
					}
				}
			}

			if (!page_frames.contains(address)) {
				// PAGE FAULT!
				page_faults++;
				if (pos < frames) {
					// There's no need for eviction
					pos++;
				} else {
					// Frames are full, need to evict the page with lowest counter
					long remove_address = lowest_counter_address(page_table, page_frames);
					total_writes = remove_page(remove_address, page_table, page_frames, total_writes);
				}
				// Add the new page
				add_page(address, page_table, page_frames, mode, 0);
			} else {
				// PAGE HIT!
				if (mode == 's')
					page_table.get(address).setDirtyBit(true);
				// Page was referenced
				page_table.get(address).setReferenceBit(true);
			}

			memory_accesses++;
		}
		trace_scan.close();
		print_results(memory_accesses, frames, page_faults, total_writes, "aging");
	}

	/*
	 *********************************** HELPER METHODS************************************
	 */

	/*
	 * Initializes the page table.
	 */

	public Hashtable<Long, PTE> init_table() {
		page_table = new Hashtable<Long, PTE>();
		for (long i = 0; i < PAGES; i++)
			page_table.put((long) i, new PTE());
		return page_table;
	}

	/*
	 * Add a new page to the frames.
	 */

	public PTE add_page(long add_address, Hashtable<Long, PTE> page_table, ArrayList<Long> page_frames, char mode,
			int frame) {
		page_frames.add(add_address);
		PTE new_entry = page_table.get(add_address);
		new_entry.setReferenceBit(false);
		new_entry.setValidBit(true);
		new_entry.setFrame(frame);
		new_entry.setIndex(add_address);
		new_entry.setAge(next_age(0, true));
		if (mode == 's')
			new_entry.setDirtyBit(true);
		page_table.put(add_address, new_entry);
		return new_entry;
	}

	/*
	 * Removes the page from the frames.
	 */

	public int remove_page(long remove_address, Hashtable<Long, PTE> page_table, ArrayList<Long> page_frames,
			int writes) {
		PTE remove = page_table.get(remove_address);
		if (remove.dirty)
			writes++;
		remove.setValidBit(false);
		remove.setDirtyBit(false);
		remove.setReferenceBit(false);
		remove.setAge(0);
		remove.setFrame(-1);
		remove.setIndex(-1);
		page_frames.remove(new Long(remove_address));
		page_table.put(remove_address, remove);
		return writes;
	}

	/*
	 * Calculates the age for a page.
	 */

	public int next_age(int val, boolean reference) {
		int next = 0;
		next = val >>> 1;
		if (reference)
			next = next | 128;
		return next;
	}

	/*
	 * Finds the page with lowest counter. [AGING]
	 */

	public long lowest_counter_address(Hashtable<Long, PTE> table, ArrayList<Long> frames) {
		int lowest = 256;
		long return_address = 0;
		for (long address : frames) {
			if (table.get(address).getAge() < lowest) {
				lowest = table.get(address).getAge();
				return_address = address;
			} else if (table.get(address).getAge() == lowest) {
				if ((table.get(address).dirty() == false && table.get(return_address).dirty() == true)) {
					return_address = address;
				} else if ((table.get(address).dirty() == table.get(return_address).dirty())
						&& return_address > address) {
					return_address = address;
				}
			}
		}
		return return_address;
	}

	/*
	 * Finds the page which is used the farthest next. [OPTIMAL]
	 */

	public long latest_used(Hashtable<Long, LinkedList<Integer>> table, long[] frames,
			Hashtable<Long, PTE> page_table) {
		int latest = 0;
		long return_address = -2;
		boolean first = false;

		for (long address : frames) {
			if (table.get(address).size() == 0) {
				if (!first) {
					first = true;
					return_address = address;
				}
				if ((page_table.get(address).dirty() == false && page_table.get(return_address).dirty() == true)) {
					return_address = address;
				} else if ((page_table.get(address).dirty() == page_table.get(return_address).dirty())
						&& return_address > address) {
					return_address = address;
				}
			} else {
				if (first)
					continue;
				if (table.get(address).peek() > latest) {
					latest = table.get(address).peek();
					return_address = address;
				}
			}
		}
		return return_address;
	}

	/*
	 * Parses the address from string to Long
	 */

	public long address(String str) {
		return Long.decode(str.substring(0, 7));
	}

	/*
	 * Prints the results
	 */

	public static void print_results(int accesses, int frames, int faults, int writes, String algorithm) {
		System.out.println("Algorithm: " + algorithm.toUpperCase());
		System.out.println("Number of frames: " + frames);
		System.out.println("Total memory accesses: " + accesses);
		System.out.println("Total page faults: " + faults);
		System.out.println("Total writes to disk: " + writes);
	}
}
