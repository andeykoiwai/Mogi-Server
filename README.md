# Mogi Flask Server

Aplikasi server Flask untuk menangani komunikasi dengan device MOGI, kuis edukasi, serta dashboard admin.

## Cara Deploy ke Render

1. Upload semua file ini ke GitHub.
2. Login ke [https://dashboard.render.com](https://dashboard.render.com).
3. Klik "New Web Service".
4. Pilih repo GitHub kamu.
5. Render akan otomatis membaca file `render.yaml` dan mulai build.
6. Setelah selesai, akses dari URL publik Render.

## File Penting

- `SERVER8_F.py` — Aplikasi utama Flask
- `requirements.txt` — Daftar dependency Python
- `render.yaml` — Instruksi deployment untuk Render
