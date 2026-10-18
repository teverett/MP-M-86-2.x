class AsmError(Exception):
    def __init__(self, message, filename=None, lineno=None):
        self.message = message
        self.filename = filename
        self.lineno = lineno
        where = ""
        if filename is not None:
            where = f"{filename}:{lineno}: " if lineno is not None else f"{filename}: "
        super().__init__(f"{where}{message}")
