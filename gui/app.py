import os
import sqlite3
import subprocess
import tkinter as tk
import json
from tkinter import ttk, messagebox, simpledialog

# ------------------------------------------------------------
# Transactional MVCC Filesystem - Desktop GUI
# Built with Tkinter + SQLite. Uses the existing project DB.
# ------------------------------------------------------------

BG = "#F3F6FB"
PANEL = "#FFFFFF"
NAVY = "#172554"
BLUE = "#4361EE"
BLUE_HOVER = "#3451D1"
TEXT = "#172033"
MUTED = "#68758A"
BORDER = "#E3E9F2"
GREEN = "#168568"
RED = "#C2414B"
AMBER = "#B7791F"


class MVCCFilesystemGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("MVCC Studio | Transactional Filesystem")
        self.root.geometry("1180x760")
        self.root.minsize(980, 650)
        self.root.configure(bg=BG)

        project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.db_path = os.path.join(project_dir, "database", "project.db")
        self.backend_process = None
        self._start_backend()
        self.selected_file_id = None
        self.selected_file_name = None

        self._configure_styles()
        self._build_layout()
        
        self.root.protocol("WM_DELETE_WINDOW", self.close_app)

        # Test C++ backend connection
        try:
            result = self.backend_command("STATE")
            print("C++ Backend Connected:", result)
        except Exception as error:
            messagebox.showerror("Backend Connection Error", str(error))

        self.refresh_files()

    def _start_backend(self):
        project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        exe_path = os.path.join(project_dir, "filesystem.exe")

        if not os.path.exists(exe_path):
            messagebox.showerror(
                "Backend Error",
                "filesystem.exe not found. Compile the C++ project first."
            )
            return

        try:
            self.backend_process = subprocess.Popen(
                [exe_path, "--gui"],
                cwd=project_dir,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                text=True,
                bufsize=1
            )
        except OSError as error:
            messagebox.showerror("Backend Error", str(error))

    def backend_command(self, command):
        if self.backend_process is None or self.backend_process.poll() is not None:
            raise RuntimeError("C++ backend is not running.")

        self.backend_process.stdin.write(command + "\n")
        self.backend_process.stdin.flush()

        response = self.backend_process.stdout.readline()

        if not response:
            raise RuntimeError("No response received from C++ backend.")

        return json.loads(response)

    def close_app(self):
        if self.backend_process and self.backend_process.poll() is None:
            try:
                self.backend_command("QUIT")
                self.backend_process.wait(timeout=3)
            except Exception:
                self.backend_process.terminate()

        self.root.destroy()

    # ------------------------- Styling -------------------------
    def _configure_styles(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        style.configure(
            "Treeview",
            background=PANEL,
            fieldbackground=PANEL,
            foreground=TEXT,
            rowheight=34,
            borderwidth=0,
            font=("Segoe UI", 10)
        )
        style.configure(
            "Treeview.Heading",
            background="#EEF2FA",
            foreground="#46536B",
            relief="flat",
            font=("Segoe UI", 9, "bold"),
            padding=(10, 10)
        )
        style.map(
            "Treeview",
            background=[("selected", "#DDE7FF")],
            foreground=[("selected", NAVY)]
        )
        style.configure(
            "TEntry",
            padding=9,
            fieldbackground="#FFFFFF",
            foreground=TEXT
        )
        style.configure(
            "TCombobox",
            padding=7,
            fieldbackground="#FFFFFF"
        )

    def _build_layout(self):
        self.root.title("Transactional MVCC Filesystem")
        self.root.geometry("1500x950")
        self.root.minsize(1100, 750)

        # Main background
        main = tk.Frame(self.root, bg="#F2F5FF")
        main.pack(fill="both", expand=True)

        # ---------------- HEADER ----------------
        header = tk.Frame(main, bg="#C9D5FF", height=105)
        header.pack(fill="x")
        header.pack_propagate(False)

        tk.Label(
            header, text="💻  Transactional MVCC Filesystem",
            bg="#C9D5FF", fg="#101D49",
            font=("Segoe UI", 23, "bold")
        ).pack(anchor="w", padx=28, pady=(15, 0))

        tk.Label(
            header,
            text="Advanced File System with MVCC, Snapshots, Deadlock Detection & Memory Management",
            bg="#C9D5FF", fg="#26345C",
            font=("Segoe UI", 11, "italic")
        ).pack(anchor="w", padx=85)

        self.connection_badge = tk.Label(
            header, text="● Database Connected\nReady to use!",
            bg="#E3EBFF", fg="#087B52",
            font=("Segoe UI", 10, "bold"), padx=18, pady=10
        )
        self.connection_badge.place(relx=0.98, rely=0.5, anchor="e")

        # ---------------- DASHBOARD BODY ----------------
          # Scrollable dashboard area
        scroll_container = tk.Frame(main, bg="#F2F5FF")
        scroll_container.pack(fill="both", expand=True, padx=8, pady=5)

        dashboard_canvas = tk.Canvas(
            scroll_container,
          bg="#F2F5FF",
          highlightthickness=0
        )
        dashboard_canvas.pack(side="left", fill="both", expand=True)

        scrollbar = ttk.Scrollbar(
          scroll_container,
           orient="vertical",
         command=dashboard_canvas.yview
        )
        scrollbar.pack(side="right", fill="y")

        dashboard_canvas.configure(
           yscrollcommand=scrollbar.set
        )

        body = tk.Frame(dashboard_canvas, bg="#F2F5FF")

        dashboard_window = dashboard_canvas.create_window(
            (0, 0),
          window=body,
          anchor="nw"
        )

        def update_scroll_region(event):
             dashboard_canvas.configure(
        scrollregion=dashboard_canvas.bbox("all")
    )

        def update_dashboard_width(event):
           dashboard_canvas.itemconfigure(
        dashboard_window,
        width=event.width
    )

        body.bind("<Configure>", update_scroll_region)
        dashboard_canvas.bind("<Configure>", update_dashboard_width)

      # Mouse-wheel scrolling (Windows)
        def scroll_dashboard(event):
         dashboard_canvas.yview_scroll(
        int(-1 * (event.delta / 120)),
        "units"
    )

         dashboard_canvas.bind_all("<MouseWheel>", scroll_dashboard)

      # Dashboard grid
        body.columnconfigure(0, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)
        body.rowconfigure(1, weight=1)
        body.rowconfigure(2, weight=0)

        def panel(parent, title, color, row, col):
            frame = tk.LabelFrame(
                parent, text=title,
                bg="white", fg="#14234D",
                font=("Segoe UI", 14, "bold"),
                labelanchor="nw",
                padx=10, pady=10,
                highlightbackground=color,
                highlightthickness=1
            )
            frame.grid(
                row=row, column=col, sticky="nsew",
                padx=6, pady=6
            )
            return frame

        def action(parent, text, command, color="#EAF0FF"):
            btn = tk.Button(
                parent, text=text, command=command,
                bg=color, fg="#10265C",
                activebackground="#D5E1FF",
                relief="ridge", bd=1,
                cursor="hand2",
                font=("Segoe UI", 10, "bold"),
                padx=8, pady=9
            )
            btn.pack(fill="x", pady=3)
            return btn

        # ---------------- FILE OPERATIONS ----------------
        files_panel = panel(
            body, "📁  File Operations", "#789BFF", 0, 0
        )

        files_panel.columnconfigure(0, weight=1)
        files_panel.columnconfigure(1, weight=1)

        operations = tk.Frame(files_panel, bg="white")
        operations.grid(row=0, column=0, sticky="nsew", padx=5)

        action(operations, "1.  Create File",
               self.create_file_dialog)
        action(operations, "2.  Read File",
               self.view_selected_file)
        action(operations, "3.  Update File",
               self.update_file_dialog)
        action(operations, "4.  Delete File",
               self.delete_selected_file)
        action(operations, "5.  View Files",
               self.refresh_files)
        action(operations, "6.  Version History",
               self.view_version_history)

        viewer = tk.Frame(files_panel, bg="#F5F7FF")
        viewer.grid(row=0, column=1, sticky="nsew", padx=5)

        tk.Label(
            viewer, text="File Content Viewer",
            bg="#F5F7FF", fg="#14234D",
            font=("Segoe UI", 11, "bold")
        ).pack(anchor="w", padx=8, pady=5)

        self.file_table = ttk.Treeview(
            viewer,
            columns=("file_id", "file_name", "current_version"),
            show="headings", height=4,
            selectmode="browse"
        )
        self.file_table.heading("file_id", text="ID")
        self.file_table.heading("file_name", text="FILE NAME")
        self.file_table.heading("current_version", text="VERSION")
        self.file_table.column("file_id", width=35)
        self.file_table.column("file_name", width=130)
        self.file_table.column("current_version", width=65)
        self.file_table.pack(fill="x", padx=5, pady=4)
        self.count_label = tk.Label(
           viewer,
           text="0 files",
             bg="white",
             fg="#68758A",
                font=("Segoe UI", 8)
             )
        self.count_label.pack(anchor="e", padx=8)
        self.file_table.bind(
            "<<TreeviewSelect>>", self.on_file_selected
        )

        self.selected_name_label = tk.Label(
            viewer, text="Select a file",
            bg="#EAF0FF", fg="#14234D",
            font=("Segoe UI", 9, "bold"), pady=7
        )
        self.selected_name_label.pack(fill="x", padx=5)

        self.content_box = tk.Text(
            viewer, height=7, wrap="word",
            bg="#F8FAFF", fg="#172033",
            font=("Consolas", 9),
            relief="flat", padx=8, pady=8
        )
        self.content_box.pack(fill="both", expand=True, padx=5, pady=5)
        self.content_box.insert("1.0", "Select a file and click Read File.")
        self.content_box.config(state="disabled")

        # ---------------- TRANSACTIONS ----------------
        tx_panel = panel(
            body, "🔄  Transactions", "#72C99B", 0, 1
        )

        tx_panel.columnconfigure(0, weight=1)
        tx_panel.columnconfigure(1, weight=1)

        tx_actions = tk.Frame(tx_panel, bg="white")
        tx_actions.grid(row=0, column=0, sticky="nsew", padx=5)

        action(tx_actions, "7.  Begin Transaction",
               lambda: self._dashboard_backend("BEGIN"))
        action(tx_actions, "8.  Commit Transaction",
               lambda: self._dashboard_backend("COMMIT"))
        action(tx_actions, "9.  Abort Transaction",
               lambda: self._dashboard_backend("ABORT"))
        action(tx_actions, "10. Transaction History",
               lambda: self._show_database_records(
                   "Transactions", "transactions",
                   ("transaction_id", "status", "start_seq",
                    "commit_seq", "completed_at"),
                   ("id", "status", "start_seq", "commit_seq")
               ))
        action(tx_actions, "14. Select Transaction",
               self._select_transaction_dialog)

        tx_info = tk.Frame(tx_panel, bg="#E7F8EF")
        tx_info.grid(row=0, column=1, sticky="nsew", padx=5)

        tk.Label(
            tx_info, text="Current Transaction",
            bg="#E7F8EF", fg="#153F31",
            font=("Segoe UI", 11, "bold")
        ).pack(pady=12)

        self.current_tx_label = tk.Label(
            tx_info, text="None",
            bg="#E7F8EF", fg="#10251C",
            font=("Segoe UI", 20, "bold")
        )
        self.current_tx_label.pack(pady=10)

        self.tx_detail_label = tk.Label(
            tx_info, text="No active transaction",
            bg="#E7F8EF", fg="#315B48",
            font=("Segoe UI", 10)
        )
        self.tx_detail_label.pack()

        # ---------------- SNAPSHOTS ----------------
        snapshot_panel = panel(
            body, "📸  Snapshots", "#A48AFF", 1, 0
        )

        snapshot_panel.columnconfigure(0, weight=1)
        snapshot_panel.columnconfigure(1, weight=1)

        snapshot_actions = tk.Frame(snapshot_panel, bg="white")
        snapshot_actions.grid(row=0, column=0, sticky="nsew", padx=5)

        action(
    snapshot_actions,
    "11. Create Snapshot",
    self._create_snapshot_dialog
)
        action(
    snapshot_actions,
    "12. Restore Snapshot",
    self._restore_snapshot_dialog
)
        action(snapshot_actions, "13. View Snapshots",
               lambda: self._show_database_records(
                   "Snapshots", "snapshots",
                   ("snapshot_id", "snapshot_name", "created_at"),
                   ("id", "name", "created_at")
               ))

        self.snapshot_info = tk.Label(
            snapshot_panel,
            text="Available Snapshots\n\nNo snapshots loaded.",
            bg="#F1EDFF", fg="#34305F",
            font=("Segoe UI", 11), justify="center"
        )
        self.snapshot_info.grid(
            row=0, column=1, sticky="nsew", padx=5
        )

        # ---------------- DEADLOCK MANAGEMENT ----------------
        deadlock_panel = panel(
            body, "⚠️  Deadlock Management", "#F4B36B", 1, 1
        )

        deadlock_panel.columnconfigure(0, weight=1)
        deadlock_panel.columnconfigure(1, weight=1)

        deadlock_actions = tk.Frame(deadlock_panel, bg="white")
        deadlock_actions.grid(
            row=0, column=0, sticky="nsew", padx=5
        )

        action(deadlock_actions, "15. View Locks",
               self._show_locks)

        action(deadlock_actions, "16. Wait-For Graph",
               self._show_wait_for_graph)

        action(deadlock_actions, "17. Detect Deadlock",
               self._detect_deadlock)

        action(deadlock_actions, "18. Recover Deadlock",
               self._recover_deadlock)

        self.lock_info = tk.Label(
            deadlock_panel,
            text="Lock Information\n\nNo live lock data loaded.",
            bg="#FFF3E9", fg="#71461F",
            font=("Segoe UI", 10), justify="center"
        )
        self.lock_info.grid(
            row=0, column=1, sticky="nsew", padx=5
        )

        # ---------------- MEMORY MANAGEMENT ----------------
        memory_panel = tk.LabelFrame(
            body, text="🧠  Memory Management",
            bg="white", fg="#12594C",
            font=("Segoe UI", 14, "bold"),
            padx=10, pady=8,
            highlightbackground="#69C9B5",
            highlightthickness=1
        )
        memory_panel.grid(
            row=2, column=0, columnspan=2,
            sticky="nsew", padx=6, pady=6
        )

        memory_top = tk.Frame(memory_panel, bg="white")
        memory_top.pack(fill="x")

        tk.Label(
            memory_top, text="Select Algorithm:",
            bg="white", fg="#172033",
            font=("Segoe UI", 10, "bold")
        ).pack(side="left", padx=8)

        self.paging_algorithm = tk.StringVar(value="FIFO")

        tk.Radiobutton(
            memory_top, text="FIFO",
            variable=self.paging_algorithm, value="FIFO",
            bg="white", font=("Segoe UI", 10)
        ).pack(side="left", padx=5)

        tk.Radiobutton(
            memory_top, text="LRU",
            variable=self.paging_algorithm, value="LRU",
            bg="white", font=("Segoe UI", 10)
        ).pack(side="left", padx=5)

        action(
    memory_top,
    "19. Apply Algorithm",
    self._apply_paging_algorithm
).pack(side="left", padx=15)

        action(
    memory_panel,
    "20. View Frames & Statistics",
    self._view_memory_stats
).pack(fill="x", padx=8, pady=5)
        self.memory_info = tk.Label(
            memory_panel,
            text="Memory Frames: 3     |     Page Size: 4096 bytes\n"
                 "Live frame and paging statistics will be connected to C++.",
            bg="#E7F8F4", fg="#155C4F",
            font=("Segoe UI", 10), pady=9
        )
        self.memory_info.pack(fill="x", padx=8, pady=4)

        # ---------------- FOOTER ----------------
        self.status_label = tk.Label(
            main,
            text="● System Ready! Select any operation from the dashboard.",
            bg="#DDF7E6", fg="#17633B",
            font=("Segoe UI", 10, "bold"),
            anchor="w", padx=18, pady=9
        )
        self.status_label.pack(fill="x", side="bottom")

        self.total_value = tk.Label(main)
        self.version_value = tk.Label(main)
        self.status_value = tk.Label(main)
         
        self.search_var = tk.StringVar(value="")
         
        self.refresh_files()
        self._refresh_dashboard_state()

    def _nav_item(self, parent, symbol, label, command=None, active=False):
        bg = "#2B3A73" if active else NAVY
        button = tk.Button(
            parent,
            text=f"  {symbol}     {label}",
            command=command,
            bg=bg,
            fg="#FFFFFF" if active else "#B8C5E9",
            activebackground="#2B3A73",
            activeforeground="#FFFFFF",
            relief="flat",
            bd=0,
            cursor="hand2",
            anchor="w",
            padx=12,
            pady=15,
            font=("Segoe UI", 10, "bold" if active else "normal")
        )
        button.pack(fill="x", padx=8, pady=3)
        return button

    def show_file_explorer(self):
        self._highlight_nav(self.nav_files)
        self.refresh_files()
        self._set_status("File Explorer opened.", GREEN)

    def show_snapshots(self):
        self._show_database_table(
            title="Snapshots",
            table_name="snapshots",
            preferred_columns=("snapshot_id", "snapshot_name", "created_at", "description")
        )

    def show_transactions(self):
        self._show_database_table(
            title="Transactions",
            table_name="transactions",
            preferred_columns=("transaction_id", "status", "start_seq", "commit_seq", "completed_at")
        )

    def _show_database_table(self, title, table_name, preferred_columns):
        connection = None
        try:
            connection = self._connect()
            cursor = connection.cursor()
            cursor.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
                (table_name,)
            )
            if not cursor.fetchone():
                messagebox.showinfo(
                    title,
                    f"The '{table_name}' table was not found in the database yet."
                )
                return

            columns_available = self._table_columns(cursor, table_name)
            columns = [column for column in preferred_columns if column in columns_available]
            if not columns:
                columns = list(columns_available)[:5]

            query = (
                "SELECT " + ", ".join(f'"{column}"' for column in columns) +
                f' FROM "{table_name}" ORDER BY 1 DESC LIMIT 200'
            )
            cursor.execute(query)
            rows = cursor.fetchall()

            window = tk.Toplevel(self.root)
            window.title(title)
            window.geometry("850x480")
            window.configure(bg=BG)

            tk.Label(
                window, text=title, bg=BG, fg=TEXT,
                font=("Segoe UI", 17, "bold")
            ).pack(anchor="w", padx=20, pady=(18, 12))

            table = ttk.Treeview(window, columns=columns, show="headings")
            for column in columns:
                table.heading(column, text=column.replace("_", " ").upper())
                table.column(column, width=max(120, 700 // len(columns)))
            scrollbar = ttk.Scrollbar(window, orient="vertical", command=table.yview)
            table.configure(yscrollcommand=scrollbar.set)
            table.pack(side="left", fill="both", expand=True, padx=(20, 0), pady=(0, 20))
            scrollbar.pack(side="right", fill="y", padx=(0, 20), pady=(0, 20))

            for row in rows:
                table.insert("", "end", values=tuple(row))
            self._set_status(f"Opened {title.lower()}.", GREEN)

        except sqlite3.Error as error:
            messagebox.showerror(f"{title} Error", str(error))
        finally:
            if connection:
                connection.close()

    def _stat_card(self, parent, label, value, note):
        card = tk.Frame(parent, bg=PANEL, highlightbackground=BORDER, highlightthickness=1)
        card.pack(side="left", fill="x", expand=True, padx=(0, 12))
        tk.Label(
            card, text=label, bg=PANEL, fg=MUTED,
            font=("Segoe UI", 8, "bold")
        ).pack(anchor="w", padx=16, pady=(13, 5))
        value_label = tk.Label(
            card, text=value, bg=PANEL, fg=TEXT,
            font=("Segoe UI", 17, "bold")
        )
        value_label.pack(anchor="w", padx=16)
        tk.Label(
            card, text=note, bg=PANEL, fg=MUTED,
            font=("Segoe UI", 8)
        ).pack(anchor="w", padx=16, pady=(3, 13))
        return value_label

    def _button(self, parent, label, command, bg, fg="#FFFFFF"):
        return tk.Button(
            parent, text=label, command=command,
            bg=bg, fg=fg, activebackground=bg, activeforeground=fg,
            relief="flat", bd=0, cursor="hand2",
            font=("Segoe UI", 9, "bold"), padx=12, pady=10
        )

    # ------------------------- Sidebar navigation -------------------------
    def _highlight_nav(self, selected):
        nav_items = [
            self.nav_files, self.nav_history,
            self.nav_snapshots, self.nav_transactions,
            self.nav_concurrency
        ]
        for item in nav_items:
            is_selected = item is selected
            bg = "#2B3A73" if is_selected else NAVY
            item.configure(
                bg=bg,
                fg="#FFFFFF" if is_selected else "#B8C5E9",
                activebackground="#2B3A73",
                activeforeground="#FFFFFF",
                font=("Segoe UI", 10, "bold" if is_selected else "normal")
            )

    def show_concurrency(self):
        """Show concurrency controls separately from persisted transaction history.

        The current C++ lock manager stores locks and waiting requests in process
        memory, so this page intentionally does not fabricate live lock data.
        """
        self._highlight_nav(self.nav_concurrency)
        window = tk.Toplevel(self.root)
        window.title("Concurrency & Deadlock Management")
        window.geometry("1000x760")
        window.minsize(800, 600)
        window.configure(bg=BG)

        header = tk.Frame(window, bg=PANEL)
        header.pack(fill="x")
        tk.Label(
            header, text="Concurrency & Deadlocks",
            bg=PANEL, fg=TEXT, font=("Segoe UI", 18, "bold")
        ).pack(anchor="w", padx=24, pady=(18, 4))
        tk.Label(
            header,
            text="Manage file locks, blocked transactions, wait-for relationships, and deadlock recovery.",
            bg=PANEL, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", padx=24, pady=(0, 18))

        body = tk.Frame(window, bg=BG)
        body.pack(fill="both", expand=True, padx=18, pady=16)

        notice = tk.Frame(body, bg="#FFF7E6", highlightbackground="#F2D7A0", highlightthickness=1)
        notice.pack(fill="x", pady=(0, 14))
        tk.Label(
            notice,
            text=("Live lock data is currently stored in C++ process memory. Open the backend console "
                  "to view the real lock table and waiting queue, detect deadlocks, and recover. "
                  "This GUI does not display simulated or invented lock records."),
            bg="#FFF7E6", fg="#7A4B00", justify="left", wraplength=900,
            font=("Segoe UI", 9), padx=14, pady=12
        ).pack(fill="x")

        actions = tk.Frame(body, bg=BG)
        actions.pack(fill="x", pady=(0, 14))
        self._button(
            actions, "Launch C++ Backend Console", self.launch_backend_console, BLUE
        ).pack(side="left")
        self._button(
            actions, "How to Test Deadlock", 
            lambda: messagebox.showinfo(
                "Test Deadlock",
                "1. Launch the C++ Backend Console.\n"
                "2. Begin two transactions.\n"
                "3. Have each transaction lock a different file.\n"
                "4. Make each transaction request the other file.\n"
                "5. Use menu option 16 to view the wait-for graph, option 17 to detect a deadlock, "
                "and option 18 to recover.\n\n"
                "Use menu option 15 to inspect the lock table and waiting queue."
            ), GREEN
        ).pack(side="left", padx=10)

        # Summary cards make this page visibly different from Transaction History.
        cards = tk.Frame(body, bg=BG)
        cards.pack(fill="x", pady=(0, 14))
        card_data = [
            ("LOCK TABLE", "File locks held by transactions", "View in C++ console — option 15"),
            ("WAITING QUEUE", "Transactions waiting for a file lock", "View in C++ console — option 15"),
            ("WAIT-FOR GRAPH", "Edges show waiter → lock owner", "View in C++ console — option 16"),
        ]
        for index, (title, description, detail) in enumerate(card_data):
            card = tk.Frame(cards, bg=PANEL, highlightbackground=BORDER, highlightthickness=1)
            card.grid(row=0, column=index, sticky="nsew", padx=(0 if index == 0 else 7, 0 if index == 2 else 7))
            tk.Label(card, text=title, bg=PANEL, fg=BLUE, font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=14, pady=(14, 6))
            tk.Label(card, text=description, bg=PANEL, fg=TEXT, font=("Segoe UI", 9), wraplength=250, justify="left").pack(anchor="w", padx=14)
            tk.Label(card, text=detail, bg=PANEL, fg=MUTED, font=("Segoe UI", 8), wraplength=250, justify="left").pack(anchor="w", padx=14, pady=(8, 14))
            cards.grid_columnconfigure(index, weight=1, uniform="concurrency_cards")

        controls = tk.Frame(body, bg=PANEL, highlightbackground=BORDER, highlightthickness=1)
        controls.pack(fill="x", pady=(0, 14))
        tk.Label(controls, text="Deadlock controls", bg=PANEL, fg=TEXT, font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=16, pady=(12, 6))
        control_row = tk.Frame(controls, bg=PANEL)
        control_row.pack(fill="x", padx=14, pady=(0, 12))
        self._button(
            control_row, "Detect Deadlock (option 17)",
            lambda: messagebox.showinfo(
                "Detect Deadlock",
                "Deadlock detection uses the live C++ lock manager. Launch the backend console and choose option 17."
            ), BLUE
        ).pack(side="left")
        self._button(
            control_row, "Recover Deadlock (option 18)",
            lambda: messagebox.showinfo(
                "Recover Deadlock",
                "Recovery aborts a selected transaction from the detected cycle. Launch the backend console and choose option 18."
            ), GREEN
        ).pack(side="left", padx=10)

        graph_wrap = tk.Frame(body, bg=PANEL, highlightbackground=BORDER, highlightthickness=1)
        graph_wrap.pack(fill="both", expand=True)
        tk.Label(
            graph_wrap, text="Wait-for graph preview",
            bg=PANEL, fg=TEXT, font=("Segoe UI", 11, "bold")
        ).pack(anchor="w", padx=16, pady=(14, 6))
        graph_text = tk.Text(
            graph_wrap, height=5, wrap="word", relief="flat", bd=0,
            bg="#F7F9FD", fg=MUTED, font=("Consolas", 10), padx=14, pady=12
        )
        graph_text.pack(fill="both", expand=True, padx=14, pady=(0, 14))
        graph_text.insert(
            "1.0",
            "No live graph is connected to the GUI yet.\n\n"
            "Example of a deadlock cycle: T1 → T2 → T1\n"
            "This means T1 is waiting for a lock held by T2, while T2 is waiting for a lock held by T1.\n"
            "Use the C++ console's option 16 to see the actual graph for the current run."
        )
        graph_text.configure(state="disabled")

    def launch_backend_console(self):
        project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        exe_path = os.path.join(project_dir, "filesystem.exe")
        if not os.path.exists(exe_path):
            messagebox.showwarning(
                "Backend executable not found",
                "Compile the C++ backend first from the project folder:\\n\\n"
                "g++ cpp/main.cpp -o filesystem.exe -lsqlite3"
            )
            return
        try:
            flags = getattr(subprocess, "CREATE_NEW_CONSOLE", 0)
            subprocess.Popen([exe_path], cwd=project_dir, creationflags=flags)
            messagebox.showinfo(
                "Backend launched",
                "The C++ console has opened. Use its menu options 15–18 to view locks, "
                "inspect the wait-for graph, detect deadlocks, and recover deadlocks."
            )
        except OSError as error:
            messagebox.showerror("Could not launch backend", str(error))

    def show_file_explorer(self):
        self._highlight_nav(self.nav_files)
        self.refresh_files()
        self._set_status("File Explorer selected.", GREEN)

    def show_version_history(self):
        self._highlight_nav(self.nav_history)
        if self._require_selection():
            self.view_version_history()
        else:
            messagebox.showinfo(
                "Version History",
                "Select a file in File Explorer first, then click Version History."
            )
            self._set_status("Select a file to view its version history.", AMBER)

    def show_snapshots(self):
        self._highlight_nav(self.nav_snapshots)
        self._show_database_records(
            title="Snapshots",
            table_name="snapshots",
            preferred_columns=("snapshot_id", "snapshot_name", "created_at"),
            fallback_columns=("id", "name", "created_at")
        )

    def show_transactions(self):
        self._highlight_nav(self.nav_transactions)
        self._show_database_records(
            title="Transactions",
            table_name="transactions",
            preferred_columns=("transaction_id", "status", "start_seq", "commit_seq", "created_at", "completed_at"),
            fallback_columns=("id", "status", "start_seq", "commit_seq")
        )

    def _show_database_records(self, title, table_name, preferred_columns, fallback_columns):
        connection = None
        try:
            connection = self._connect()
            cursor = connection.cursor()
            cursor.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
                (table_name,)
            )
            if cursor.fetchone() is None:
                messagebox.showinfo(
                    title,
                    f"The '{table_name}' table does not exist in this database yet."
                )
                self._set_status(f"{title}: database table not found.", AMBER)
                return

            available = self._table_columns(cursor, table_name)
            columns = [name for name in preferred_columns if name in available]
            if not columns:
                columns = [name for name in fallback_columns if name in available]
            if not columns:
                columns = list(available)[:6]

            safe_columns = ", ".join('"' + name.replace('"', '""') + '"' for name in columns)
            cursor.execute(f'SELECT {safe_columns} FROM "{table_name}" ORDER BY rowid DESC LIMIT 500')
            rows = cursor.fetchall()

        except sqlite3.Error as error:
            messagebox.showerror(f"{title} Error", str(error))
            self._set_status(f"Could not load {title.lower()}.", RED)
            return
        finally:
            if connection:
                connection.close()

        window = tk.Toplevel(self.root)
        window.title(title)
        window.geometry("850x480")
        window.minsize(600, 350)
        window.configure(bg=BG)

        tk.Label(
            window, text=title, bg=BG, fg=TEXT,
            font=("Segoe UI", 17, "bold")
        ).pack(anchor="w", padx=22, pady=(20, 4))
        tk.Label(
            window, text=f"{len(rows)} record(s) from the database",
            bg=BG, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", padx=22, pady=(0, 14))

        wrapper = tk.Frame(window, bg=BG)
        wrapper.pack(fill="both", expand=True, padx=22, pady=(0, 22))
        tree = ttk.Treeview(wrapper, columns=columns, show="headings")
        y_scroll = ttk.Scrollbar(wrapper, orient="vertical", command=tree.yview)
        x_scroll = ttk.Scrollbar(wrapper, orient="horizontal", command=tree.xview)
        tree.configure(yscrollcommand=y_scroll.set, xscrollcommand=x_scroll.set)

        for column in columns:
            tree.heading(column, text=column.replace("_", " ").upper())
            tree.column(column, width=max(120, min(220, len(column) * 12)), anchor="w")

        tree.grid(row=0, column=0, sticky="nsew")
        y_scroll.grid(row=0, column=1, sticky="ns")
        x_scroll.grid(row=1, column=0, sticky="ew")
        wrapper.rowconfigure(0, weight=1)
        wrapper.columnconfigure(0, weight=1)

        for row in rows:
            tree.insert("", "end", values=tuple(row[column] for column in columns))
        self._set_status(f"Opened {title.lower()} records.", GREEN)

    # ------------------------- Database helpers -------------------------
    def _connect(self):
        if not os.path.exists(self.db_path):
            raise sqlite3.Error(f"Database not found:\n{self.db_path}")
        connection = sqlite3.connect(self.db_path, timeout=5)
        connection.row_factory = sqlite3.Row
        return connection

    def _table_columns(self, cursor, table_name):
        cursor.execute(f"PRAGMA table_info({table_name})")
        return {row["name"] for row in cursor.fetchall()}

    def _set_status(self, message, color=MUTED):
        self.status_label.config(text=message, fg=color)
        self.status_value.config(text="Ready" if color != RED else "Error")

    def _filter_files(self):
        query = self.search_var.get().strip().lower()
        for item in self.file_table.get_children():
            self.file_table.delete(item)
        filtered = [
            row for row in getattr(self, "file_rows", [])
            if query in str(row["file_name"]).lower()
        ]
        for row in filtered:
            self.file_table.insert(
                "", "end",
                values=(row["file_id"], row["file_name"], row["current_version"])
            )
        self.count_label.config(text=f"{len(filtered)} items")

    def refresh_files(self):
        connection = None
        try:
            connection = self._connect()
            cursor = connection.cursor()
            cursor.execute("""
                SELECT file_id, file_name, current_version
                FROM files
                WHERE COALESCE(is_deleted, 0) = 0
                ORDER BY file_id DESC
            """)
            self.file_rows = cursor.fetchall()
            self._filter_files()
            self.total_value.config(text=str(len(self.file_rows)))
            self._set_status(f"Loaded {len(self.file_rows)} file(s).", GREEN)
            self.connection_badge.config(text="●  DATABASE CONNECTED", bg="#E7F7F0", fg=GREEN)
        except sqlite3.Error as error:
            self.connection_badge.config(text="●  CONNECTION ERROR", bg="#FCEBED", fg=RED)
            self._set_status("Could not load files.", RED)
            messagebox.showerror("Database Error", str(error))
        finally:
            if connection:
                connection.close()

    def on_file_selected(self, _event=None):
        selected = self.file_table.selection()
        if not selected:
            return
        values = self.file_table.item(selected[0], "values")
        self.selected_file_id = int(values[0])
        self.selected_file_name = values[1]
        self.selected_name_label.config(
            text=f"{values[1]}   •   Version {values[2]}",
            fg=TEXT
        )
        self.version_value.config(text=str(values[2]))
        self._set_status(f"Selected {values[1]}.")

    def _require_selection(self):
        selected = self.file_table.selection()
        if not selected:
            messagebox.showwarning("Select a file", "Please select a file from the list first.")
            return None
        values = self.file_table.item(selected[0], "values")
        self.selected_file_id = int(values[0])
        self.selected_file_name = values[1]
        return self.selected_file_id, self.selected_file_name

    # ------------------------- Read operations -------------------------
    def view_selected_file(self):
        selection = self._require_selection()
        if not selection:
            return
        file_id, file_name = selection
        connection = None

        try:
            connection = self._connect()
            cursor = connection.cursor()
            columns = self._table_columns(cursor, "file_versions")
            committed_filter = "AND COALESCE(fv.is_committed, 1) = 1" if "is_committed" in columns else ""

            cursor.execute(f"""
                SELECT fv.version_number, fv.content
                FROM file_versions fv
                WHERE fv.file_id = ?
                {committed_filter}
                ORDER BY fv.version_number DESC
                LIMIT 1
            """, (file_id,))
            row = cursor.fetchone()
            if row:
                paging_result = self.backend_command(
                    f"LOADFILE {file_id} {file_name}"
                )

                print("LOADFILE RESPONSE:", paging_result)

                if not paging_result.get("ok"):
                    messagebox.showwarning(
                        "Paging Warning",
                        paging_result.get(
                            "message",
                            "File could not be loaded into memory."
                        )
                    )

            self.content_box.config(state="normal")
            self.content_box.delete("1.0", tk.END)
            if row:
                self.content_box.insert(
                    "1.0",
                    f"FILE  /  {file_name}\n"
                    f"VERSION  /  {row['version_number']}\n"
                    f"{'─' * 32}\n\n{row['content'] or '(Empty file)'}"
                )
                self._set_status(f"Viewing {file_name}, version {row['version_number']}.", GREEN)
            else:
                self.content_box.insert("1.0", "No committed content found for this file.")
                self._set_status("No committed version found.", AMBER)
            self.content_box.config(state="disabled")
        except sqlite3.Error as error:
            messagebox.showerror("Read Error", str(error))
            self._set_status("Could not read file.", RED)
        finally:
            if connection:
                connection.close()

    def view_version_history(self):
        selection = self._require_selection()
        if not selection:
            return
        file_id, file_name = selection
        connection = None

        try:
            connection = self._connect()
            cursor = connection.cursor()
            columns = self._table_columns(cursor, "file_versions")
            committed_filter = "AND COALESCE(is_committed, 1) = 1" if "is_committed" in columns else ""
            cursor.execute(f"""
                SELECT version_number, content
                FROM file_versions
                WHERE file_id = ? {committed_filter}
                ORDER BY version_number DESC
            """, (file_id,))
            rows = cursor.fetchall()
        except sqlite3.Error as error:
            messagebox.showerror("History Error", str(error))
            return
        finally:
            if connection:
                connection.close()

        window = tk.Toplevel(self.root)
        window.title(f"Version History | {file_name}")
        window.geometry("720x500")
        window.configure(bg=BG)

        tk.Label(
            window, text=f"Version history  /  {file_name}",
            bg=BG, fg=TEXT, font=("Segoe UI", 15, "bold")
        ).pack(anchor="w", padx=20, pady=(18, 5))
        tk.Label(
            window, text=f"{len(rows)} version(s) recorded",
            bg=BG, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", padx=20, pady=(0, 12))

        cols = ("version", "preview")
        table = ttk.Treeview(window, columns=cols, show="headings")
        table.heading("version", text="VERSION")
        table.heading("preview", text="CONTENT PREVIEW")
        table.column("version", width=100, anchor="center")
        table.column("preview", width=560)
        table.pack(fill="both", expand=True, padx=20, pady=(0, 20))

        for row in rows:
            content = (row["content"] or "").replace("\n", " ")
            if len(content) > 100:
                content = content[:100] + "…"
            table.insert("", "end", values=(row["version_number"], content))

    # ------------------------- Write operations -------------------------
    # Writes create a new version instead of overwriting old content.
    def create_file_dialog(self):
        dialog = tk.Toplevel(self.root)
        dialog.title("Create a file")
        dialog.geometry("500x390")
        dialog.resizable(False, False)
        dialog.configure(bg=BG)
        dialog.transient(self.root)
        dialog.grab_set()

        tk.Label(
            dialog, text="Create a new file",
            bg=BG, fg=TEXT, font=("Segoe UI", 17, "bold")
        ).pack(anchor="w", padx=24, pady=(22, 4))
        tk.Label(
            dialog, text="A new file and its first version will be saved to SQLite.",
            bg=BG, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", padx=24, pady=(0, 16))

        tk.Label(dialog, text="File name", bg=BG, fg=TEXT, font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=24)
        name_entry = ttk.Entry(dialog)
        name_entry.pack(fill="x", padx=24, pady=(5, 14))

        tk.Label(dialog, text="Initial content", bg=BG, fg=TEXT, font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=24)
        content_text = tk.Text(dialog, height=7, wrap="word", font=("Consolas", 10), bd=0, padx=10, pady=8)
        content_text.pack(fill="both", expand=True, padx=24, pady=(5, 14))

        def save():
            name = name_entry.get().strip()
            content = content_text.get("1.0", "end-1c")
            if not name:
                messagebox.showwarning("Missing name", "Enter a file name.", parent=dialog)
                return
            if "/" in name or "\\" in name:
                messagebox.showwarning("Invalid name", "Enter a file name, not a path.", parent=dialog)
                return
            connection = None
            try:
                connection = self._connect()
                cursor = connection.cursor()
                file_cols = self._table_columns(cursor, "files")
                values = {}
                if "file_name" in file_cols:
                    values["file_name"] = name
                if "current_version" in file_cols:
                    values["current_version"] = 1
                if "is_deleted" in file_cols:
                    values["is_deleted"] = 0
                if "created_at" in file_cols:
                    values["created_at"] = self._now_sql()
                if "updated_at" in file_cols:
                    values["updated_at"] = self._now_sql()

                cursor.execute(
                    f"INSERT INTO files ({', '.join(values)}) VALUES ({', '.join('?' for _ in values)})",
                    tuple(values.values())
                )
                file_id = cursor.lastrowid

                version_cols = self._table_columns(cursor, "file_versions")
                version_values = {}
                possible = {
                    "file_id": file_id,
                    "version_number": 1,
                    "content": content,
                    "is_committed": 1,
                    "transaction_id": None,
                    "commit_seq": None,
                    "created_at": self._now_sql(),
                }
                for col, value in possible.items():
                    if col in version_cols:
                        version_values[col] = value

                cursor.execute(
                    f"INSERT INTO file_versions ({', '.join(version_values)}) VALUES ({', '.join('?' for _ in version_values)})",
                    tuple(version_values.values())
                )
                connection.commit()
                dialog.destroy()
                self.refresh_files()
                self._set_status(f"Created {name}.", GREEN)
            except sqlite3.Error as error:
                if connection:
                    connection.rollback()
                messagebox.showerror(
                    "Create Error",
                    "Could not create the file.\n\n"
                    f"{error}\n\n"
                    "Your database schema may require additional fields."
                , parent=dialog)
            finally:
                if connection:
                    connection.close()

        self._button(dialog, "Save File", save, BLUE).pack(fill="x", padx=24, pady=(0, 20))
        name_entry.focus_set()

    def update_file_dialog(self):
        selection = self._require_selection()
        if not selection:
            return
        file_id, file_name = selection

        dialog = tk.Toplevel(self.root)
        dialog.title(f"Update {file_name}")
        dialog.geometry("600x500")
        dialog.configure(bg=BG)
        dialog.transient(self.root)
        dialog.grab_set()

        tk.Label(
            dialog, text=f"Create a new version",
            bg=BG, fg=TEXT, font=("Segoe UI", 17, "bold")
        ).pack(anchor="w", padx=24, pady=(22, 4))
        tk.Label(
            dialog, text=f"File: {file_name}  •  Existing versions are preserved",
            bg=BG, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", padx=24, pady=(0, 15))

        initial = ""
        connection = None
        try:
            connection = self._connect()
            cursor = connection.cursor()

            columns = self._table_columns(cursor, "file_versions")
            committed_filter = (
                "AND COALESCE(is_committed, 1) = 1"
                if "is_committed" in columns
                else ""
            )

            cursor.execute(f"""
                SELECT content FROM file_versions
                WHERE file_id = ? {committed_filter}
                ORDER BY version_number DESC
                LIMIT 1
            """, (file_id,))

            row = cursor.fetchone()
            initial = ""

            if row:
                paging_result = self.backend_command(
                    f"LOADFILE {file_id} {file_name}"
                )

                if not paging_result.get("ok"):
                    messagebox.showwarning(
                        "Paging Warning",
                        paging_result.get(
                            "message",
                            "File could not be loaded into memory."
                        ),
                        parent=dialog
                    )

                initial = row["content"] or ""

        except sqlite3.Error as error:
            messagebox.showerror(
                "Read Error",
                str(error),
                parent=dialog
            )
            dialog.destroy()
            return

        finally:
            if connection:
                connection.close()

        tk.Label(
            dialog,
            text="New content",
            bg=BG,
            fg=TEXT,
            font=("Segoe UI", 9, "bold")
        ).pack(anchor="w", padx=24)

        content_text = tk.Text(
            dialog,
            height=10,
            wrap="word",
            font=("Consolas", 10),
            bd=0,
            padx=12,
            pady=10
        )

        content_text.pack(
            fill="both",
            expand=True,
            padx=24,
            pady=(5, 10)
        )

        content_text.insert("1.0", initial)

         
        def save():
            content = content_text.get("1.0", "end-1c")

            try:
                # Encode filename and content for safe backend communication
                encoded_name = file_name.encode("utf-8").hex()
                encoded_content = content.encode("utf-8").hex() or "-"

                # Send update to the C++ transaction manager
                result = self.backend_command(
                    f"UPDATE {encoded_name} {encoded_content}"
                )

                message = result.get(
                    "message",
                    "Update operation completed."
                )

                if result.get("ok"):
                    dialog.destroy()
                    self.refresh_files()
                    self._select_file_by_id(file_id)

                    self._set_status(
                        "Update staged successfully. Commit the transaction to save it.",
                        GREEN
                    )

                    messagebox.showinfo(
                        "Update Staged",
                        message + "\n\nCommit the transaction to make this update permanent."
                    )

                else:
                    messagebox.showwarning(
                        "Update Failed",
                        message,
                        parent=dialog
                    )

            except Exception as error:
                messagebox.showerror(
                    "Update Error",
                    str(error),
                    parent=dialog
                )


        self._button(dialog, "Save New Version", save, BLUE).pack(fill="x", padx=24, pady=(0, 20))

    def delete_selected_file(self):
        selection = self._require_selection()
        if not selection:
            return
        file_id, file_name = selection

        if not messagebox.askyesno(
            "Delete file",
            f"Delete '{file_name}' from the active file list?\n\n"
            "The file will be soft-deleted if your schema supports it."
        ):
            return

        connection = None
        try:
            connection = self._connect()
            cursor = connection.cursor()
            columns = self._table_columns(cursor, "files")
            if "is_deleted" in columns:
                cursor.execute("UPDATE files SET is_deleted = 1 WHERE file_id = ?", (file_id,))
            else:
                cursor.execute("DELETE FROM files WHERE file_id = ?", (file_id,))
            connection.commit()
            self.content_box.config(state="normal")
            self.content_box.delete("1.0", tk.END)
            self.content_box.config(state="disabled")
            self.refresh_files()
            self._set_status(f"Deleted {file_name}.", GREEN)
        except sqlite3.Error as error:
            if connection:
                connection.rollback()
            messagebox.showerror("Delete Error", str(error))
            self._set_status("Could not delete file.", RED)
        finally:
            if connection:
                connection.close()

    def _select_file_by_id(self, file_id):
        for item in self.file_table.get_children():
            values = self.file_table.item(item, "values")
            if values and int(values[0]) == int(file_id):
                self.file_table.selection_set(item)
                self.file_table.focus(item)
                self.file_table.see(item)
                self.on_file_selected()
                break
     
    def _create_snapshot_dialog(self):
        name = simpledialog.askstring(
            "Create Snapshot",
            "Enter snapshot name:",
            parent=self.root
        )

        if name is None:
            return

        name = name.strip()

        if not name:
            messagebox.showwarning(
                "Invalid Name",
                "Snapshot name cannot be empty."
            )
            return

        try:
            result = self.backend_command("SNAPSHOT " + name)
            message = result.get("message", "Snapshot operation completed.")

            if result.get("ok"):
                self._set_status(message, GREEN)
                messagebox.showinfo("Snapshot Created", message)
            else:
                self._set_status(message, RED)
                messagebox.showwarning("Snapshot Failed", message)

        except Exception as error:
            messagebox.showerror("Snapshot Error", str(error))
            self._set_status("Snapshot creation failed.", RED)
     
    def _restore_snapshot_dialog(self):

        name = simpledialog.askstring(
            "Restore Snapshot",
            "Enter the snapshot name to restore:",
            parent=self.root
        )

        if name is None:
            return

        name = name.strip()

        if not name:
            messagebox.showwarning(
                "Invalid Name",
                "Snapshot name cannot be empty."
            )
            return

        confirm = messagebox.askyesno(
            "Confirm Restore",
            f"Are you sure you want to restore '{name}'?\n\n"
            "Current committed files will be replaced."
        )

        if not confirm:
            return

        try:
            result = self.backend_command("RESTORE " + name)
            message = result.get(
                "message",
                "Snapshot restoration completed."
            )

            if result.get("ok"):
                self.refresh_files()
                self._refresh_dashboard_state()
                self._set_status(message, GREEN)

                messagebox.showinfo(
                    "Restore Successful",
                    message
                )
            else:
                self._set_status(message, RED)

                messagebox.showwarning(
                    "Restore Failed",
                    message
                )

        except Exception as error:
            messagebox.showerror(
                "Restore Error",
                str(error)
            )
            self._set_status("Snapshot restoration failed.", RED)

    def _not_connected(self, feature):
        messagebox.showinfo(
            feature,
            f"{feature} is visible in the dashboard, "
            "but its GUI backend command has not been implemented yet."
        )

    def _dashboard_backend(self, command):
        try:
            result = self.backend_command(command)
            message = result.get("message", "Operation completed.")
            self._set_status(message, GREEN)
            self._refresh_dashboard_state()
            messagebox.showinfo("Backend Response", message)
        except Exception as error:
            messagebox.showerror("Backend Error", str(error))
            self._set_status("Backend operation failed.", RED)
    def _detect_deadlock(self):
        try:
           result = self.backend_command("DETECT")

           print("DETECT RESPONSE:", result)

           message = result.get("message", "No message received.")

           messagebox.showinfo(
            "Deadlock Detection Result",
             message
        )

        except Exception as error:
            messagebox.showerror(
            "Detection Error",
            str(error)
        )
    def _show_locks(self):
        try:
            result = self.backend_command("STATE")

            locks = result.get("locks", [])
            waiting = result.get("waiting", [])

            window = tk.Toplevel(self.root)
            window.title("Lock Information")
            window.geometry("650x450")
            window.configure(bg=BG)

            tk.Label(
                window,
                text="Current Lock Information",
                bg=BG, fg=TEXT,
                font=("Segoe UI", 16, "bold")
            ).pack(anchor="w", padx=20, pady=15)

            tk.Label(
                window,
                text=f"Locks Held: {len(locks)}",
                bg=BG, fg=TEXT,
                font=("Segoe UI", 10, "bold")
            ).pack(anchor="w", padx=20)

            lock_table = ttk.Treeview(
                window,
                columns=("file", "transaction"),
                show="headings",
                height=5
            )

            lock_table.heading("file", text="RESOURCE / FILE")
            lock_table.heading("transaction", text="LOCK HOLDER")
            lock_table.pack(fill="x", padx=20, pady=8)

            for item in locks:
                lock_table.insert(
                    "",
                    "end",
                    values=(
                        item.get("file", "-"),
                        f"T{item.get('transaction_id', '-')}"
                    )
                )

            tk.Label(
                window,
                text=f"Waiting Requests: {len(waiting)}",
                bg=BG, fg=TEXT,
                font=("Segoe UI", 10, "bold")
            ).pack(anchor="w", padx=20, pady=(12, 0))

            waiting_table = ttk.Treeview(
                window,
                columns=("file", "transaction"),
                show="headings",
                height=5
            )

            waiting_table.heading("file", text="RESOURCE / FILE")
            waiting_table.heading("transaction", text="WAITING TRANSACTION")
            waiting_table.pack(fill="x", padx=20, pady=8)

            for item in waiting:
                waiting_table.insert(
                    "",
                    "end",
                    values=(
                        item.get("file", "-"),
                        f"T{item.get('transaction_id', '-')}"
                    )
                )

            self._refresh_dashboard_state()

        except Exception as error:
            messagebox.showerror("View Locks Error", str(error))

    def _show_wait_for_graph(self):
        try:
            result = self.backend_command("STATE")
            edges = result.get("edges", [])

            window = tk.Toplevel(self.root)
            window.title("Wait-For Graph")
            window.geometry("600x400")
            window.configure(bg=BG)

            tk.Label(
                window,
                text="Wait-For Graph",
                bg=BG, fg=TEXT,
                font=("Segoe UI", 16, "bold")
            ).pack(anchor="w", padx=20, pady=15)

            graph_table = ttk.Treeview(
                window,
                columns=("waiting", "holding", "file"),
                show="headings"
            )

            graph_table.heading("waiting", text="WAITING TRANSACTION")
            graph_table.heading("holding", text="LOCK HOLDER")
            graph_table.heading("file", text="RESOURCE / FILE")

            graph_table.column("waiting", width=150)
            graph_table.column("holding", width=150)
            graph_table.column("file", width=200)

            graph_table.pack(
                fill="both",
                expand=True,
                padx=20,
                pady=10
            )

            for edge in edges:
                graph_table.insert(
                    "",
                    "end",
                    values=(
                        f"T{edge.get('from', '-')}",
                        f"T{edge.get('to', '-')}",
                        edge.get("file", "-")
                    )
                )

            if not edges:
                tk.Label(
                    window,
                    text="No wait-for relationships found.",
                    bg=BG, fg=MUTED,
                    font=("Segoe UI", 10)
                ).pack(pady=10)

            self._refresh_dashboard_state()

        except Exception as error:
            messagebox.showerror(
                "Wait-For Graph Error",
                str(error)
            )

    def _detect_deadlock(self):
        self._dashboard_backend("DETECT")

    def _recover_deadlock(self):
        self._dashboard_backend("RECOVER")
    def _refresh_dashboard_state(self):
        try:
            result = self.backend_command("STATE")

            # Update selected transaction
            selected = result.get("selected_transaction", -1)

            if selected == -1:
                self.current_tx_label.config(text="None")
                self.tx_detail_label.config(
                    text="No active transaction"
                )
            else:
                self.current_tx_label.config(text=f"T{selected}")
                self.tx_detail_label.config(
                    text="Transaction selected"
                )

            # Get deadlock information
            locks = result.get("locks", [])
            waiting = result.get("waiting", [])
            edges = result.get("edges", [])

            # Format lock information
            lock_details = (
                "\n".join(f"  • {item}" for item in locks)
                if locks else "  No locks currently held"
            )

            waiting_details = (
                "\n".join(f"  • {item}" for item in waiting)
                if waiting else "  No waiting requests"
            )

            edge_details = (
                "\n".join(f"  • {item}" for item in edges)
                if edges else "  No wait-for edges"
            )

            # Display complete information
            self.lock_info.config(
                text=(
                    "DEADLOCK MANAGEMENT\n\n"
                    f"Locks Held: {len(locks)}\n"
                    f"Waiting Requests: {len(waiting)}\n"
                    f"Wait-for Edges: {len(edges)}\n\n"
                    "LOCK DETAILS\n"
                    f"{lock_details}\n\n"
                    "WAITING REQUEST DETAILS\n"
                    f"{waiting_details}\n\n"
                    "WAIT-FOR GRAPH DETAILS\n"
                    f"{edge_details}"
                )
            )

            # Update backend status
            self.connection_badge.config(
                text="● Backend Connected",
                fg="#16A34A"
            )

        except Exception as error:
            self.connection_badge.config(
                text="● Backend Disconnected",
                fg="#C2414B"
            )

            self._set_status(str(error), RED)
    def _apply_paging_algorithm(self):
        algorithm = self.paging_algorithm.get()

        try:
            result = self.backend_command("PAGING " + algorithm)
            print("PAGING BACKEND RESPONSE:", result)

            message = result.get("message", "Paging algorithm updated.")

            if result.get("ok"):
                self.memory_info.config(
                    text=f"Memory Frames: 3     |     Page Size: 4096 bytes\n"
                         f"Selected Algorithm: {algorithm}"
                )

                self._set_status(message.strip(), GREEN)
                messagebox.showinfo("Paging Algorithm", message)

            else:
                self._set_status(message, RED)
                messagebox.showwarning("Paging Algorithm", message)

        except Exception as error:
            messagebox.showerror("Paging Error", str(error))
    
    
    def _view_memory_stats(self):
        try:
            result = self.backend_command("PAGINGSTATE")

            if not result.get("ok"):
                messagebox.showerror(
                    "Memory Error",
                    result.get("message", "Unable to retrieve memory statistics.")
                )
                return

            algorithm = result.get("algorithm", "FIFO")
            page_size = result.get("page_size", 4096)
            hits = result.get("hits", 0)
            faults = result.get("faults", 0)
            replacements = result.get("replacements", 0)

            frames = result.get("frames", [])
            frame_text = ""

            for frame in frames:
                if frame.get("occupied"):
                    frame_text += (
                        f"Frame {frame['frame']}: "
                        f"{frame['file']} | Page {frame['page_number']}\n"
                    )
                else:
                    frame_text += f"Frame {frame['frame']}: Empty\n"

            display_text = (
                f"Paging Algorithm: {algorithm}\n"
                f"Page Size: {page_size} bytes\n\n"
                f"MEMORY FRAMES\n"
                f"{frame_text}\n"
                f"PAGE STATISTICS\n"
                f"Page Hits: {hits}\n"
                f"Page Faults: {faults}\n"
                f"Page Replacements: {replacements}"
            )

            self.memory_info.config(text=display_text)
            self._set_status("Memory statistics refreshed.", GREEN)

        except Exception as error:
            messagebox.showerror("Memory Error", str(error))

           
    def _select_transaction_dialog(self):
        try:
            connection = self._connect()
            cursor = connection.cursor()
            cursor.execute(
                "SELECT transaction_id, status FROM transactions "
                "WHERE status = 'ACTIVE' ORDER BY transaction_id"
            )
            rows = cursor.fetchall()
            connection.close()

            if not rows:
                messagebox.showinfo(
                    "Select Transaction",
                    "No active transactions found. Begin a transaction first."
                )
                return

            window = tk.Toplevel(self.root)
            window.title("Select Transaction")
            window.geometry("350x250")

            tk.Label(
                window, text="Choose an active transaction",
                font=("Segoe UI", 11, "bold")
            ).pack(pady=12)

            choices = [str(row["transaction_id"]) for row in rows]
            selected = tk.StringVar(value=choices[0])

            ttk.Combobox(
                window, textvariable=selected,
                values=choices, state="readonly"
            ).pack(pady=10)

            def choose():
                self._dashboard_backend("SELECT " + selected.get())
                window.destroy()

            tk.Button(
                window, text="Select",
                command=choose, bg="#4361EE",
                fg="white", padx=20, pady=7
            ).pack(pady=12)

        except sqlite3.Error as error:
            messagebox.showerror("Transaction Error", str(error))

    @staticmethod
    def _now_sql():
        from datetime import datetime
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


if __name__ == "__main__":
    root = tk.Tk()
    app = MVCCFilesystemGUI(root)
    root.mainloop()
