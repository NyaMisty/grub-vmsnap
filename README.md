# Grub module for extracting vmware snapshots

It will parse the VM's current snapshot path and output it in space-splited format.

## Usage

```
insmod vmsnap
vmsnap [--set=XXX] VMX_file_path
```
- --set will pass the final output into varable specified

## Example

vmsnap --set=snapshots /TestVM.vmx