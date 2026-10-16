import os
import sqlite3
import subprocess
import tkinter as tk
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
        self.selected_file_id = None
        self.selected_file_name = None

        self._configure_styles()
        self._build_layout()
        self.refresh_files()

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
        # Main app shell
        shell = tk.Frame(self.root, bg=BG)
        shell.pack(fill="both", expand=True)

        # Left navigation
        sidebar = tk.Frame(shell, bg=NAVY, width=225)
        sidebar.pack(side="left", fill="y")
        sidebar.pack_propagate(False)

        brand = tk.Frame(sidebar, bg=NAVY)
        brand.pack(fill="x", padx=22, pady=(26, 30))

        tk.Label(
            brand, text="◈  MVCC STUDIO",
            bg=NAVY, fg="#FFFFFF",
            font=("Segoe UI", 15, "bold")
        ).pack(anchor="w")

        tk.Label(
            brand, text="TRANSACTIONAL FILESYSTEM",
            bg=NAVY, fg="#AAB9E8",
            font=("Segoe UI", 8, "bold")
        ).pack(anchor="w", pady=(7, 0))

        tk.Label(
            sidebar, text="WORKSPACE",
            bg=NAVY, fg="#8292C7",
            font=("Segoe UI", 8, "bold")
        ).pack(anchor="w", padx=22, pady=(0, 10))

        self.nav_files = self._nav_item(
            sidebar, "▦", "File Explorer",
            command=self.show_file_explorer, active=True
        )
        self.nav_history = self._nav_item(
            sidebar, "◷", "Version History",
            command=self.show_version_history
        )
        self.nav_snapshots = self._nav_item(
            sidebar, "▣", "Snapshots",
            command=self.show_snapshots
        )
        self.nav_transactions = self._nav_item(
            sidebar, "⇄", "Transactions",
            command=self.show_transactions
        )
        self.nav_concurrency = self._nav_item(
            sidebar, "⟳", "Concurrency & Deadlocks",
            command=self.show_concurrency
        )

        spacer = tk.Frame(sidebar, bg=NAVY)
        spacer.pack(fill="both", expand=True)

        tk.Frame(sidebar, bg="#34416F", height=1).pack(fill="x", padx=18, pady=12)
        tk.Label(
            sidebar, text="SQLite connected",
            bg=NAVY, fg="#B8C7EF",
            font=("Segoe UI", 9)
        ).pack(anchor="w", padx=22, pady=(0, 5))
        tk.Label(
            sidebar, text="Local project database",
            bg=NAVY, fg="#8292C7",
            font=("Segoe UI", 8)
        ).pack(anchor="w", padx=22, pady=(0, 22))

        # Main content
        content = tk.Frame(shell, bg=BG)
        content.pack(side="left", fill="both", expand=True)

        # Header
        header = tk.Frame(content, bg=PANEL, height=104)
        header.pack(fill="x")
        header.pack_propagate(False)

        header_left = tk.Frame(header, bg=PANEL)
        header_left.pack(side="left", padx=28, pady=13)

        tk.Label(
            header_left, text="File Explorer",
            bg=PANEL, fg=TEXT, font=("Segoe UI", 18, "bold")
        ).pack(anchor="w")
        tk.Label(
            header_left, text="Manage files and inspect committed versions",
            bg=PANEL, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", pady=(3, 0))

        self.connection_badge = tk.Label(
            header, text="●  DATABASE",
            bg="#E7F7F0", fg=GREEN,
            font=("Segoe UI", 9, "bold"), padx=12, pady=7
        )
        self.connection_badge.pack(side="right", padx=28)

        # Scrollable-ish main area
        body = tk.Frame(content, bg=BG)
        body.pack(fill="both", expand=True, padx=25, pady=22)

        # Summary cards
        cards = tk.Frame(body, bg=BG)
        cards.pack(fill="x", pady=(0, 18))

        self.total_value = self._stat_card(cards, "TOTAL FILES", "—", "Visible files")
        self.version_value = self._stat_card(cards, "SELECTED VERSION", "—", "Latest committed")
        self.status_value = self._stat_card(cards, "SYSTEM STATUS", "Ready", "SQLite database")

        # Two-column work area
        work = tk.Frame(body, bg=BG)
        work.pack(fill="both", expand=True)

        left = tk.Frame(work, bg=PANEL, highlightbackground=BORDER, highlightthickness=1)
        left.pack(side="left", fill="both", expand=True, padx=(0, 14))

        list_header = tk.Frame(left, bg=PANEL)
        list_header.pack(fill="x", padx=18, pady=(18, 10))

        tk.Label(
            list_header, text="Your files",
            bg=PANEL, fg=TEXT, font=("Segoe UI", 12, "bold")
        ).pack(side="left")

        self.count_label = tk.Label(
            list_header, text="0 items",
            bg=PANEL, fg=MUTED, font=("Segoe UI", 9)
        )
        self.count_label.pack(side="right")

        search_row = tk.Frame(left, bg=PANEL)
        search_row.pack(fill="x", padx=18, pady=(0, 12))

        self.search_var = tk.StringVar()
        self.search_var.trace_add("write", lambda *_: self._filter_files())
        self.search_entry = ttk.Entry(
            search_row, textvariable=self.search_var
        )
        self.search_entry.pack(side="left", fill="x", expand=True)
        self.search_entry.insert(0, "")

        ttk.Button(
            search_row, text="↻ Refresh",
            command=self.refresh_files
        ).pack(side="left", padx=(8, 0))

        table_wrap = tk.Frame(left, bg=PANEL)
        table_wrap.pack(fill="both", expand=True, padx=18, pady=(0, 18))

        columns = ("file_id", "file_name", "current_version")
        self.file_table = ttk.Treeview(
            table_wrap, columns=columns, show="headings", selectmode="browse"
        )
        self.file_table.heading("file_id", text="ID")
        self.file_table.heading("file_name", text="FILE NAME")
        self.file_table.heading("current_version", text="VERSION")
        self.file_table.column("file_id", width=55, anchor="center", stretch=False)
        self.file_table.column("file_name", width=230)
        self.file_table.column("current_version", width=85, anchor="center", stretch=False)

        scrollbar = ttk.Scrollbar(table_wrap, orient="vertical", command=self.file_table.yview)
        self.file_table.configure(yscrollcommand=scrollbar.set)
        self.file_table.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        self.file_table.bind("<<TreeviewSelect>>", self.on_file_selected)
        self.file_table.bind("<Double-1>", lambda _event: self.view_selected_file())

        # Right panel
        right = tk.Frame(work, bg=PANEL, width=355, highlightbackground=BORDER, highlightthickness=1)
        right.pack(side="right", fill="y")
        right.pack_propagate(False)

        tk.Label(
            right, text="File details",
            bg=PANEL, fg=TEXT, font=("Segoe UI", 12, "bold")
        ).pack(anchor="w", padx=18, pady=(18, 3))

        tk.Label(
            right, text="Select a file to inspect its content.",
            bg=PANEL, fg=MUTED, font=("Segoe UI", 9)
        ).pack(anchor="w", padx=18, pady=(0, 12))

        self.selected_name_label = tk.Label(
            right, text="No file selected",
            bg="#F4F7FC", fg=MUTED,
            font=("Segoe UI", 10, "bold"),
            anchor="w", padx=12, pady=11
        )
        self.selected_name_label.pack(fill="x", padx=18, pady=(0, 12))

        self.content_box = tk.Text(
            right, height=13, wrap="word",
            bg="#F8FAFD", fg=TEXT,
            relief="flat", bd=0,
            padx=12, pady=12,
            font=("Consolas", 10),
            insertbackground=BLUE
        )
        self.content_box.pack(fill="both", expand=True, padx=18, pady=(0, 12))
        self.content_box.insert("1.0", "Choose a file from the list, then click View Content.")
        self.content_box.config(state="disabled")

        action_frame = tk.Frame(right, bg=PANEL)
        action_frame.pack(fill="x", padx=18, pady=(0, 18))

        self._button(action_frame, "View Content", self.view_selected_file, BLUE).pack(
            fill="x", pady=(0, 7)
        )
        self._button(action_frame, "View Version History", self.view_version_history, "#EAF0FF", BLUE).pack(
            fill="x", pady=(0, 7)
        )
        self._button(action_frame, "Create New File", self.create_file_dialog, "#E7F7F0", GREEN).pack(
            fill="x", pady=(0, 7)
        )
        self._button(action_frame, "Update Selected File", self.update_file_dialog, "#FFF4DF", AMBER).pack(
            fill="x", pady=(0, 7)
        )
        self._button(action_frame, "Delete Selected File", self.delete_selected_file, "#FCEBED", RED).pack(
            fill="x"
        )

        # Footer status bar
        footer = tk.Frame(content, bg="#E8EDF5", height=30)
        footer.pack(fill="x", side="bottom")
        footer.pack_propagate(False)
        self.status_label = tk.Label(
            footer, text="Ready",
            bg="#E8EDF5", fg=MUTED,
            font=("Segoe UI", 9), anchor="w"
        )
        self.status_label.pack(fill="both", padx=25)

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
            committed_filter = "AND COALESCE(is_committed, 1) = 1" if "is_committed" in columns else ""
            cursor.execute(f"""
                SELECT content FROM file_versions
                WHERE file_id = ? {committed_filter}
                ORDER BY version_number DESC LIMIT 1
            """, (file_id,))
            row = cursor.fetchone()
            if row:
                initial = row["content"] or ""
        except sqlite3.Error as error:
            messagebox.showerror("Read Error", str(error), parent=dialog)
            dialog.destroy()
            return
        finally:
            if connection:
                connection.close()

        tk.Label(dialog, text="New content", bg=BG, fg=TEXT, font=("Segoe UI", 9, "bold")).pack(anchor="w", padx=24)
        content_text = tk.Text(dialog, wrap="word", font=("Consolas", 10), bd=0, padx=12, pady=10)
        content_text.pack(fill="both", expand=True, padx=24, pady=(5, 15))
        content_text.insert("1.0", initial)

        def save():
            content = content_text.get("1.0", "end-1c")
            connection2 = None
            try:
                connection2 = self._connect()
                cursor2 = connection2.cursor()
                cursor2.execute(
                    "SELECT COALESCE(MAX(version_number), 0) + 1 AS next_version "
                    "FROM file_versions WHERE file_id = ?",
                    (file_id,)
                )
                next_version = cursor2.fetchone()["next_version"]

                file_cols = self._table_columns(cursor2, "files")
                version_cols = self._table_columns(cursor2, "file_versions")
                version_values = {}
                possible = {
                    "file_id": file_id,
                    "version_number": next_version,
                    "content": content,
                    "is_committed": 1,
                    "transaction_id": None,
                    "commit_seq": None,
                    "created_at": self._now_sql(),
                }
                for col, value in possible.items():
                    if col in version_cols:
                        version_values[col] = value

                cursor2.execute(
                    f"INSERT INTO file_versions ({', '.join(version_values)}) VALUES ({', '.join('?' for _ in version_values)})",
                    tuple(version_values.values())
                )

                updates = {}
                if "current_version" in file_cols:
                    updates["current_version"] = next_version
                if "updated_at" in file_cols:
                    updates["updated_at"] = self._now_sql()
                if updates:
                    cursor2.execute(
                        f"UPDATE files SET {', '.join(f'{col} = ?' for col in updates)} WHERE file_id = ?",
                        (*updates.values(), file_id)
                    )

                connection2.commit()
                dialog.destroy()
                self.refresh_files()
                self._set_status(f"Saved {file_name} as version {next_version}.", GREEN)
                self._select_file_by_id(file_id)
                self.view_selected_file()
            except sqlite3.Error as error:
                if connection2:
                    connection2.rollback()
                messagebox.showerror(
                    "Update Error",
                    "Could not save a new version.\n\n"
                    f"{error}\n\n"
                    "Your database schema may require additional fields.",
                    parent=dialog
                )
            finally:
                if connection2:
                    connection2.close()

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

    @staticmethod
    def _now_sql():
        from datetime import datetime
        return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


if __name__ == "__main__":
    root = tk.Tk()
    app = MVCCFilesystemGUI(root)
    root.mainloop()
