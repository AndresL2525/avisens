# =============================================================================
# AVÍSENS Backend - Dockerfile
# =============================================================================
# Multi-stage build para optimizar tamaño de imagen:
#   Stage 1: Builder (instala dependencias y compila)
#   Stage 2: Runtime (imagen final ligera)
# =============================================================================

# -----------------------------------------------------------------------------
# STAGE 1: Builder
# -----------------------------------------------------------------------------
FROM python:3.11-slim AS builder

WORKDIR /app

# Instalar herramientas de compilación (necesarias para algunas librerías)
RUN apt-get update && apt-get install -y --no-install-recommends     gcc     libffi-dev     && rm -rf /var/lib/apt/lists/*

# Crear entorno virtual (mejor práctica: aislar dependencias del sistema)
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Instalar dependencias
COPY requirements.txt .
RUN pip install --no-cache-dir --upgrade pip &&     pip install --no-cache-dir -r requirements.txt

# -----------------------------------------------------------------------------
# STAGE 2: Runtime
# -----------------------------------------------------------------------------
FROM python:3.11-slim

WORKDIR /app

# Crear usuario no-root (seguridad: el contenedor no corre como root)
RUN groupadd -r avisens && useradd -r -g avisens avisens

# Copiar solo el entorno virtual desde el builder
COPY --from=builder /opt/venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Copiar código de la aplicación
COPY app/ ./app/

# Ajustar permisos
RUN chown -R avisens:avisens /app

# Cambiar a usuario no-root
USER avisens

# Puerto expuesto (documentativo, no restrictivo)
EXPOSE 8000

# Health check: verifica que el servidor responde cada 30 segundos
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3     CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')" || exit 1

# Comando de inicio
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
