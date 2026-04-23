Import("env")

def skip_asm(node):
    path = node.get_path()
    if path.endswith(".S") or path.endswith(".s"):
        if "helium" in path or "neon" in path or "riscv" in path:
            return None
    return node

env.AddBuildMiddleware(skip_asm)
