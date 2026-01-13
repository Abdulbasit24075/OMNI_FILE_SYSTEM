import sys
import socket
import json
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QLineEdit, QPushButton, 
                             QTreeWidget, QTreeWidgetItem, QTableWidget, QTableWidgetItem,
                             QHeaderView, QMessageBox, QInputDialog, QDialog, QFormLayout,
                             QSplitter, QFrame, QComboBox, QTextEdit, QMenu)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QIcon, QFont, QColor

# Configuration
SERVER_IP = "127.0.0.1"
SERVER_PORT = 8081

class OFSClientGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("OFS File Explorer")
        self.resize(900, 600)
        
        # State
        self.session_id = ""
        self.current_path = "/" # Default Root
        self.username = ""
        self.is_admin = False

        # START WITH LOGIN SCREEN
        self.show_login_screen()

    def send_request(self, operation, params=None):
        """Helper to send JSON requests to C++ Server"""
        if params is None: params = {}
        
        req = {
            "operation": operation,
            "request_id": "gui_req",
            "parameters": params
        }
        
        # Attach session if logged in
        if self.session_id:
            req["session_id"] = self.session_id

        try:
            client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client.settimeout(2.0)
            client.connect((SERVER_IP, SERVER_PORT))
            client.sendall(json.dumps(req).encode('utf-8'))
            
            response = client.recv(16384) # Large buffer for file lists
            client.close()
            
            return json.loads(response.decode('utf-8'))
        except Exception as e:
            return {"status": "error", "error_message": str(e)}

    # =========================================================================
    # UI CONSTRUCTION
    # =========================================================================

    def show_login_screen(self):
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        layout = QVBoxLayout(self.central_widget)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)

        # Title
        title = QLabel("OFS File System Login")
        title.setFont(QFont("Arial", 20, QFont.Weight.Bold))
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)

        # Form
        form_frame = QFrame()
        form_frame.setFixedWidth(300)
        form_layout = QVBoxLayout(form_frame)
        
        self.user_input = QLineEdit()
        self.user_input.setPlaceholderText("Username")
        self.pass_input = QLineEdit()
        self.pass_input.setPlaceholderText("Password")
        self.pass_input.setEchoMode(QLineEdit.EchoMode.Password)
        
        login_btn = QPushButton("Login")
        login_btn.setStyleSheet("background-color: #007BFF; color: white; padding: 10px; font-weight: bold;")
        login_btn.clicked.connect(self.perform_login)

        form_layout.addWidget(self.user_input)
        form_layout.addWidget(self.pass_input)
        form_layout.addWidget(login_btn)
        
        layout.addWidget(form_frame)

    def show_main_interface(self):
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)
        main_layout = QVBoxLayout(self.central_widget)

        # --- TOP BAR ---
        top_bar = QHBoxLayout()
        
        self.path_label = QLabel("/")
        self.path_label.setStyleSheet("background-color: #e0e0e0; padding: 5px; border-radius: 4px;")
        
        btn_up = QPushButton("⬆ Up")
        btn_up.clicked.connect(self.go_up)
        
        btn_refresh = QPushButton("🔄 Refresh")
        btn_refresh.clicked.connect(self.refresh_files)
        
        btn_create_file = QPushButton("📄 New File")
        btn_create_file.clicked.connect(self.create_file_dialog)

        btn_create_dir = QPushButton("📁 New Folder")
        btn_create_dir.clicked.connect(self.create_dir_dialog)

        btn_logout = QPushButton("Logout")
        btn_logout.setStyleSheet("color: red;")
        btn_logout.clicked.connect(self.logout)

        top_bar.addWidget(QLabel("Path:"))
        top_bar.addWidget(self.path_label, 1) # Stretch
        top_bar.addWidget(btn_up)
        top_bar.addWidget(btn_refresh)
        top_bar.addWidget(btn_create_file)
        top_bar.addWidget(btn_create_dir)
        top_bar.addWidget(btn_logout)
        
        main_layout.addLayout(top_bar)

        # --- SPLITTER (Explorer vs Admin Panel) ---
        splitter = QSplitter(Qt.Orientation.Horizontal)
        
        # LEFT: File Table
        self.file_table = QTableWidget()
        self.file_table.setColumnCount(3)
        self.file_table.setHorizontalHeaderLabels(["Name", "Type", "Size"])
        self.file_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.file_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.file_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        self.file_table.cellDoubleClicked.connect(self.on_file_double_click)
        
        # Add Context Menu (Right Click)
        self.file_table.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        self.file_table.customContextMenuRequested.connect(self.show_context_menu)

        splitter.addWidget(self.file_table)

        # RIGHT: Admin Panel (Only visible if Admin)
        if(self.is_admin):
            self.admin_panel = QFrame()
            self.admin_panel.setFrameShape(QFrame.Shape.StyledPanel)
            self.setup_admin_panel(self.admin_panel)
            splitter.addWidget(self.admin_panel)
        
        main_layout.addWidget(splitter)
        
        # Load initial path
        self.refresh_files()

    def setup_admin_panel(self, panel):
        layout = QVBoxLayout(panel)
        title = QLabel("Admin Controls")
        title.setFont(QFont("Arial", 12, QFont.Weight.Bold))
        layout.addWidget(title)
        
        # User Management
        layout.addWidget(QLabel("Create User:"))
        self.new_user_input = QLineEdit()
        self.new_user_input.setPlaceholderText("Username")
        self.new_pass_input = QLineEdit()
        self.new_pass_input.setPlaceholderText("Password")
        btn_add_user = QPushButton("Add User")
        btn_add_user.clicked.connect(self.admin_create_user)
        
        layout.addWidget(self.new_user_input)
        layout.addWidget(self.new_pass_input)
        layout.addWidget(btn_add_user)
        
        layout.addWidget(QLabel("System Stats:"))
        self.stats_label = QLabel("Loading...")
        layout.addWidget(self.stats_label)
        
        btn_refresh_stats = QPushButton("Refresh Stats")
        btn_refresh_stats.clicked.connect(self.admin_get_stats)
        layout.addWidget(btn_refresh_stats)
        
        layout.addStretch()

    # =========================================================================
    # LOGIC
    # =========================================================================

    def perform_login(self):
        u = self.user_input.text()
        p = self.pass_input.text()
        
        resp = self.send_request("user_login", {"username": u, "password": p})
        
        if resp.get("status") == "success":
            self.session_id = resp["data"]["session_id"]
            self.username = u
            # Check if admin (Simple check for Phase 1, ideally server returns role)
            self.is_admin = (u == "admin")
            
            # FIX 1: RESET PATH TO ROOT ON LOGIN
            self.current_path = "/" 
            
            self.show_main_interface()
            if self.is_admin:
                self.admin_get_stats()
        else:
            # FIX 2: SECURITY - DO NOT LOGIN IF FAILED
            self.session_id = "" # Clear any accidental session
            QMessageBox.critical(self, "Login Failed", resp.get("error_message", "Unknown Error"))

    def logout(self):
        # FIX 1: RESET STATE ON LOGOUT
        self.session_id = ""
        self.username = ""
        self.current_path = "/"
        self.is_admin = False
        self.show_login_screen()

    def refresh_files(self):
        resp = self.send_request("dir_list", {"path": self.current_path})
        
        if resp.get("status") == "success":
            files = resp["data"]["files"]
            self.file_table.setRowCount(len(files))
            
            for i, f in enumerate(files):
                # Name
                name_item = QTableWidgetItem(f["name"])
                
                # Icon
                if f["type"] == "dir":
                    name_item.setIcon(QIcon.fromTheme("folder"))
                    name_item.setForeground(QColor("blue"))
                else:
                    name_item.setIcon(QIcon.fromTheme("text-x-generic"))
                
                self.file_table.setItem(i, 0, name_item)
                self.file_table.setItem(i, 1, QTableWidgetItem(f["type"]))
                self.file_table.setItem(i, 2, QTableWidgetItem(str(f.get("size", 0)) + " B"))
                
            self.path_label.setText(self.current_path)
        else:
            # Graceful fail if path doesn't exist (e.g. after deletion)
            if "not found" in resp.get("error_message", "").lower():
                self.current_path = "/"
                self.refresh_files()
            else:
                QMessageBox.warning(self, "Error", resp.get("error_message", "Failed to list dir"))

    def on_file_double_click(self, row, col):
        name = self.file_table.item(row, 0).text()
        type_Str = self.file_table.item(row, 1).text()
        
        if type_Str == "dir":
            # Navigate Down
            if self.current_path == "/":
                self.current_path += name
            else:
                self.current_path += "/" + name
            self.refresh_files()
        else:
            # Read File
            self.read_file_content(name)

    def go_up(self):
        if self.current_path == "/":
            return
        
        # Remove last segment
        self.current_path = self.current_path[:self.current_path.rfind("/")]
        if self.current_path == "":
            self.current_path = "/"
            
        self.refresh_files()

    def create_file_dialog(self):
        name, ok = QInputDialog.getText(self, "New File", "Filename:")
        if ok and name:
            content, ok2 = QInputDialog.getMultiLineText(self, "File Content", "Enter content:")
            if ok2:
                full_path = (self.current_path + "/" + name).replace("//", "/")
                resp = self.send_request("file_create", {
                    "path": full_path,
                    "type": "file",
                    "data": content
                })
                if resp.get("status") == "success":
                    self.refresh_files()
                else:
                    QMessageBox.critical(self, "Error", resp.get("error_message"))

    def create_dir_dialog(self):
        name, ok = QInputDialog.getText(self, "New Folder", "Folder Name:")
        if ok and name:
            full_path = (self.current_path + "/" + name).replace("//", "/")
            # Reuse file_create with type=dir
            resp = self.send_request("file_create", {
                "path": full_path,
                "type": "dir",
                "data": ""
            })
            if resp.get("status") == "success":
                self.refresh_files()
            else:
                QMessageBox.critical(self, "Error", resp.get("error_message"))

    def read_file_content(self, filename):
        full_path = (self.current_path + "/" + filename).replace("//", "/")
        resp = self.send_request("file_read", {"path": full_path})
        
        if resp.get("status") == "success":
            content = resp["data"]["content"]
            # Show in dialog
            dlg = QDialog(self)
            dlg.setWindowTitle(filename)
            layout = QVBoxLayout(dlg)
            text = QTextEdit()
            text.setText(content)
            text.setReadOnly(True)
            layout.addWidget(text)
            dlg.exec()
        else:
            QMessageBox.critical(self, "Error", resp.get("error_message"))

    def show_context_menu(self, pos):
        index = self.file_table.indexAt(pos)
        if not index.isValid(): return
        
        row = index.row()
        name = self.file_table.item(row, 0).text()
        type_str = self.file_table.item(row, 1).text()
        
        menu = QMenu(self)
        delete_action = menu.addAction("Delete")
        action = menu.exec(self.file_table.viewport().mapToGlobal(pos))
        
        if action == delete_action:
            reply = QMessageBox.question(self, 'Confirm Delete', f"Are you sure you want to delete '{name}'?", 
                                         QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
            
            if reply == QMessageBox.StandardButton.Yes:
                full_path = (self.current_path + "/" + name).replace("//", "/")
                # Determine operation based on type
                op = "dir_delete" if type_str == "dir" else "file_delete"
                
                resp = self.send_request(op, {"path": full_path})
                
                if resp.get("status") == "success":
                    QMessageBox.information(self, "Success", "Deleted successfully")
                    self.refresh_files()
                else:
                    QMessageBox.critical(self, "Error", resp.get("error_message"))

    # --- ADMIN FUNCTIONS ---
    def admin_create_user(self):
        u = self.new_user_input.text()
        p = self.new_pass_input.text()
        if not u or not p: return
        
        resp = self.send_request("user_create", {"username": u, "password": p})
        if resp.get("status") == "success":
            QMessageBox.information(self, "Success", "User Created!")
            self.new_user_input.clear()
            self.new_pass_input.clear()
            self.admin_get_stats()
        else:
            QMessageBox.critical(self, "Error", resp.get("error_message"))

    def admin_get_stats(self):
        resp = self.send_request("get_stats")
        if resp.get("status") == "success":
            s = resp["data"]["stats"]
            text = f"Total Size: {s['total_size']}\nUsed: {s['used_space']}\nFree: {s['free_space']}\nFiles: {s['total_files']}\nDirs: {s['total_directories']}"
            self.stats_label.setText(text)

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = OFSClientGUI()
    window.show()
    sys.exit(app.exec())