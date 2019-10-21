public class PTE {
	long index;
	int frame;
	int age;
	
	boolean reference;
	boolean valid;
	boolean dirty;
	
	public PTE() {
		//this.index = -1;
		this.age = 0;
		this.frame = -1;
		this.reference = false;
		this.valid = false;
		this.dirty = false;
	}
	
	public PTE(PTE swap){
		this.index = swap.index;
		this.age = swap.age;
		this.frame = swap.frame;
		this.reference = swap.reference;
		this.valid = swap.valid;
		this.dirty = swap.dirty;
	}

	
	public boolean referenced() {
		return this.reference;
	}
	
	public boolean dirty() {
		return this.dirty;
	}
	
	public boolean valid() {
		return this.valid;
	}
	
	public long getIndex() {
		return this.index;
	}
	
	public int getAge() {
		return this.age;
	}
	
	public int getFrame() {
		return this.frame;
	}
	
	public void setReferenceBit(boolean bit) {
		this.reference = bit;
	}
	
	public void setDirtyBit(boolean bit) {
		this.dirty = bit;
	}
	
	public void setValidBit(boolean bit) {
		this.valid = bit;
	}
	
	public void setIndex(long index) {
		this.index = index;
	}
	
	public void setAge(int age) {
		this.age = age;
	}
	
	public void setFrame(int frame) {
		this.frame = frame;
	}
}
