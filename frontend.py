import sys
import socket
import struct
from PyQt5.QtWidgets import (
    QApplication, QWidget, QLabel, QLineEdit, QPushButton, QVBoxLayout, QMessageBox, QDialog
)
from PyQt5.QtGui import QFont
from PyQt5.QtCore import Qt

SERVER_HOST = '127.0.0.1'
SERVER_PORT = 12345

class WinDialog(QDialog):
    def __init__(self, title, message, parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.setFixedSize(400, 300)

        label = QLabel(message, self)
        label.setObjectName("winDialogLabel")
        label.setAlignment(Qt.AlignCenter)
        label.setWordWrap(True)
        label.setStyleSheet("font-size: 24px; font-weight: bold; color: #27ae60;")  # green text

        btn = QPushButton("Close", self)
        btn.clicked.connect(self.accept)
        btn.setStyleSheet("""
            QPushButton {
                font-size: 16px;
                background-color: #2980B9;
                color: white;
                border-radius: 8px;
                padding: 10px 20px;
            }
            QPushButton:hover {
                background-color: #3498DB;
            }
        """)

        layout = QVBoxLayout()
        layout.addWidget(label)
        layout.addWidget(btn, alignment=Qt.AlignCenter)
        self.setLayout(layout)

class HangmanClient(QWidget):
    def __init__(self):
        super().__init__()
        self.setFixedSize(640, 480)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((SERVER_HOST, SERVER_PORT))
        except Exception as e:
            QMessageBox.critical(self, "Connection Error", str(e))
            sys.exit(1)
            
        self.initUI()
        self.start_game()
        self.centerWindow()

    def initUI(self):
        self.word_label = QLabel("Word: ", self)
        self.word_label.setObjectName("wordLabel")

        self.wrong_label = QLabel("Incorrect guesses: ", self)
        self.wrong_label.setObjectName("wrongLabel")

        self.guess_input = QLineEdit(self)
        self.guess_input.setMaxLength(1)
        self.guess_input.setObjectName("guessInput")
        self.guess_input.setAlignment(Qt.AlignCenter)

        self.guess_btn = QPushButton("Submit Guess", self)
        self.guess_btn.setObjectName("guessButton")
        self.guess_btn.clicked.connect(self.send_guess)

        layout = QVBoxLayout()
        layout.setContentsMargins(40, 40, 40, 40)
        layout.setSpacing(20)
        layout.addWidget(self.word_label, alignment=Qt.AlignCenter)
        layout.addWidget(self.wrong_label, alignment=Qt.AlignCenter)
        layout.addWidget(self.guess_input, alignment=Qt.AlignCenter)
        layout.addWidget(self.guess_btn, alignment=Qt.AlignCenter)
        self.setLayout(layout)

    def centerWindow(self):
        frame_gm = self.frameGeometry()
        screen_center = QApplication.desktop().availableGeometry().center()
        frame_gm.moveCenter(screen_center)
        self.move(frame_gm.topLeft())

    def start_game(self):
        raw_data = self.sock.recv(76)
        msg_flag, _, _, _ = struct.unpack('!iii64s', raw_data)
        if msg_flag == -1:
            QMessageBox.warning(self, "Server Full", "Server is overloaded.")
            self.close()
            return

        self.sock.send(struct.pack("!i64s", 0, b''))  # start signal
        self.update_game_state()

    def update_game_state(self):
        raw_data = self.sock.recv(76)
        msg_flag, word_len, num_incorrect, data = struct.unpack('<iii64s', raw_data)

        data = data.decode(errors='ignore').rstrip('\x00')
        word_display = data[:word_len]
        wrong_guesses = data[word_len:word_len + num_incorrect]
        
        #print(msg_flag, word_len, num_incorrect) for debugging

        if msg_flag == 100:
            # Expect second message with win/lose string length flag next
            raw_data = self.sock.recv(76)
            msg_flag2, _, _, data2 = struct.unpack('<iii64s', raw_data)
            data2 = data2.decode(errors='ignore').rstrip('\x00')

            if msg_flag2 == 8:
                dlg = WinDialog(
                    "You Win!",
                    f"Congrats!\nYou correctly guessed:\n{word_display}",
                    self
                )
                dlg.exec_()
                self._end_game()
                return
            
            elif msg_flag2 == 9:
                dlg = WinDialog(
                    "You Lose!",
                    f"Game Over!\nThe correct word was:\n{word_display}",
                    self
                )
                dlg.findChild(QLabel, "winDialogLabel").setStyleSheet(
                    "font-size: 24px; font-weight: bold; color: #c0392b;"
                )
                dlg.exec_()
                self._end_game()
                return

        elif msg_flag > 0:
            # Other result cases
            QMessageBox.information(self, "Result", word_display)
            self._end_game()
            return

        self.word_label.setText(f"Word: {' '.join(word_display)}")
        self.wrong_label.setText(f"Incorrect guesses: {' '.join(wrong_guesses)}")
        self.guess_input.setText("")

    def _end_game(self):
        self.guess_input.setDisabled(True)
        self.guess_btn.setDisabled(True)

    def send_guess(self):
        guess = self.guess_input.text().strip().lower()
        if not guess.isalpha() or len(guess) != 1:
            QMessageBox.warning(self, "Invalid Input", "Please enter a single letter.")
            return

        msg = struct.pack("!i64s", 1, guess.encode().ljust(64, b'\x00'))
        self.sock.send(msg)
        self.update_game_state()

    def closeEvent(self, event):
        try:
            msg = struct.pack("!i64s", -5, b'')
            self.sock.send(msg)
            self.sock.close()
        except:
            pass
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)

    # Load external stylesheet
    with open("stylesheet.qss", "r") as f:
        app.setStyleSheet(f.read())

    client = HangmanClient()
    client.show()
    sys.exit(app.exec_())
