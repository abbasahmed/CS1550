import java.io.File;
import java.io.FileNotFoundException;

public class vmsim {

	public static void main(String[] args) {
		Algorithms run = new Algorithms();

		int frames = 0;
		String algorithm = "";
		int refresh = 0;

		if (args.length < 5) {
			System.out.println("\nPlease follow the syntax:");
			System.out.println("./vmsim –n <numframes> -a <opt|fifo|aging> [-r <refresh>] <tracefile>\n");
			return;
		}

		for (int i = 0; i < args.length; ++i) {
			if (args[i].equalsIgnoreCase("-n")) {
				frames = Integer.parseInt(args[i + 1]);
			} else if (args[i].equalsIgnoreCase("-a")) {
				algorithm = args[i + 1].toLowerCase();
			} else if (args[i].equalsIgnoreCase("-r")) {
				refresh = Integer.parseInt(args[i + 1]);
			}
		}
		
		if(algorithm.equals("opt")) {
			try {
				File trace_file = new File(args[args.length - 1]);
				run.opt(frames, trace_file);
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			}
		} else if(algorithm.equals("fifo")) {
			try {
				File trace_file = new File(args[args.length - 1]);
				run.fifo(frames, trace_file);
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			}
		} else if(algorithm.contentEquals("aging")) {
			try {
				File trace_file = new File(args[args.length - 1]);
				run.aging(frames, trace_file, refresh);
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			}
		} else {
			System.out.println("Please follow the syntax:\n");
			System.out.println("./vmsim –n <numframes> -a <opt|fifo|aging> [-r <refresh>] <tracefile>\n");
		}
	}
}