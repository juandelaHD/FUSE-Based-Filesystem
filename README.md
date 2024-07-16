# FUSE-Based-Filesystem
Implementing a custom Linux filesystem using FUSE, allowing user-mode implementation with VFS interface access via standard syscalls (read, open, ls, etc.). This in-memory filesystem prioritizes speed over data volume or persistence, with data represented on disk by a single file.
