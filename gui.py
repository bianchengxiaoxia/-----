import sys
import subprocess
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QTextEdit, QLineEdit, QPushButton
from PyQt5.QtCore import QThread, pyqtSignal, QByteArray

class ClientThread(QThread):
    new_message = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.process = None

    def run(self):
        # 启动client.exe，与C++客户端交互
        self.process = subprocess.Popen(["./client"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
        while True:
            output = self.process.stdout.readline()
            if output:
                self.new_message.emit(output.strip())
            if self.process.poll() is not None:
                break

    def send_message(self, message):
        if self.process and self.process.stdin:
            self.process.stdin.write(message + "\n")
            self.process.stdin.flush()

    def stop(self):
        if self.process:
            self.process.terminate()
            self.process = None

class ChatClient(QWidget):
    def __init__(self):
        super().__init__()
        self.init_ui()
        self.client_thread = ClientThread()
        self.client_thread.new_message.connect(self.receive_message)
        self.client_thread.start()

    def init_ui(self):
        self.setWindowTitle("Chat Client")
        self.resize(400, 300)

        self.layout = QVBoxLayout()
        self.setLayout(self.layout)

        self.message_display = QTextEdit(self)
        self.message_display.setReadOnly(True)
        self.layout.addWidget(self.message_display)

        self.message_input = QLineEdit(self)
        self.layout.addWidget(self.message_input)

        self.send_button = QPushButton("Send", self)
        self.layout.addWidget(self.send_button)

        self.send_button.clicked.connect(self.send_message)
        self.message_input.returnPressed.connect(self.send_message)

    def send_message(self):
        message = self.message_input.text()
        if not message:
            return

        self.client_thread.send_message(message)
        self.message_input.clear()

    def receive_message(self, message):
        self.message_display.append(message)

    def closeEvent(self, event):
        self.client_thread.stop()
        self.client_thread.wait()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    client = ChatClient()
    client.show()
    sys.exit(app.exec_())
