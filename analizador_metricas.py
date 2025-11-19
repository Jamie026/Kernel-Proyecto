import pandas as pd
from tabulate import tabulate
import json
import os

# ===============================================
# 1. CONFIGURACIÓN
# ===============================================

# --- DEFINE AQUÍ EL NÚMERO DE ESCENARIO A ANALIZAR ---
# Asegúrate de que los archivos metricas_mision_[N].json y metricas_total_[N].json existan.
ESCENARIO_NUMERO = 3

# ===============================================
# 2. FUNCIONES DE CARGA DE DATOS
# ===============================================

def cargar_datos_json(escenario):
    """Carga los datos detallados y acumulados desde los archivos JSON para un escenario dado."""
    
    datos_mision = None
    datos_total = None
    
    # 1. Cargar Archivo de Métricas Detalladas por Ciclo
    nombre_mision = f"metricas_mision_{escenario}.json"
    if not os.path.exists(nombre_mision):
        print(f"\n❌ Error: Archivo de misión '{nombre_mision}' no encontrado.")
        return None, None

    try:
        with open(nombre_mision, 'r') as f:
            datos_mision = json.load(f)
        print(f"✅ Archivo de misión '{nombre_mision}' cargado correctamente.")
    except json.JSONDecodeError:
        print(f"❌ Error: El archivo '{nombre_mision}' no es un JSON válido.")
        return None, None
        
    # 2. Cargar Archivo de Métricas Totales Acumuladas
    nombre_total = f"metricas_total_{escenario}.json"
    if not os.path.exists(nombre_total):
        print(f"\n❌ Error: Archivo total '{nombre_total}' no encontrado.")
        return None, None

    try:
        with open(nombre_total, 'r') as f:
            datos_total_bruto = json.load(f)
            
            # Adaptar la estructura del JSON total al diccionario simple usado para la tabla
            datos_total = {
                "Escenario Analizado": datos_total_bruto.get("escenario", escenario),
                "Total de Ciclos Acumulados": datos_total_bruto.get("total_ciclos_acumulados", 0),
                "Tiempo Real Total (Wall Time)": datos_total_bruto.get("tiempo_real_total", 0.0),
                "Tiempo Total de CPU (Usuario + Sistema)": datos_total_bruto.get("tiempo_total_cpu", 0.0),
                "CPU Usuario Acumulado": datos_total_bruto.get("cpu_usuario_acumulado", 0.0),
                "CPU Sistema Acumulado": datos_total_bruto.get("cpu_sistema_acumulado", 0.0),
                "Memoria Total (Suma de Picos)": datos_total_bruto.get("memoria_pico_total_kb", 0)
            }
        print(f"✅ Archivo total '{nombre_total}' cargado correctamente.")
    except json.JSONDecodeError:
        print(f"❌ Error: El archivo '{nombre_total}' no es un JSON válido.")
        return None, None

    return datos_mision, datos_total

# ===============================================
# 3. FUNCIONES DE GENERACIÓN DE TABLAS
# ===============================================

def generar_tabla_acumulada(data):
    """Genera una tabla a partir de los datos acumulados."""
    df_acumulado = pd.DataFrame(
        list(data.items()), 
        columns=["Métrica", "Valor"]
    )
    
    # Formatear el valor
    df_acumulado['Valor'] = df_acumulado.apply(
        lambda row: f"{row['Valor']:.6f} s" if 'Tiempo' in row['Métrica'] else 
                    f"{row['Valor']}" if 'Ciclos' in row['Métrica'] or 'Escenario' in row['Métrica'] else 
                    f"{row['Valor']:,} KB", 
        axis=1
    )
    
    print("\n" + "="*80)
    print("🤖 REPORTE ACUMULADO GLOBAL 🤖".center(80))
    print("="*80)
    print(tabulate(df_acumulado, headers="keys", tablefmt="fancy_grid", showindex=False))
    print("="*80 + "\n")

def generar_tabla_detallada(data):
    """Extrae y genera una tabla detallada por ciclo y proceso."""
    
    # 1. Normalizar el JSON y extraer las métricas clave
    registros = []
    for ciclo_data in data:
        ciclo = ciclo_data['ciclo']
        
        # Iterar sobre P1, P2 y P3
        for nombre_proceso, p_data in ciclo_data['procesos'].items():
            
            # Asignar un nombre legible
            if nombre_proceso == 'proceso1':
                nombre_display = 'P1 (Receptor)'
            elif nombre_proceso == 'proceso2':
                nombre_display = 'P2 (Escudo)'
            else:
                nombre_display = 'P3 (Analizador)'
                
            # Calcular tiempo pausado (es 0 para P2 si no está en las métricas)
            tiempo_pausado = p_data.get('tiempo_pausado_total', 0.0)
            num_pausas = p_data.get('num_pausas', 0)
            
            registros.append({
                'Ciclo': ciclo,
                'Proceso': nombre_display,
                'T. Total (Wall)': ciclo_data['tiempo_total_ciclo'],
                'T. Muerto Kernel': ciclo_data['tiempo_muerto_kernel'],
                'CPU Efectiva (s)': p_data['ejecucion_efectiva'],
                'T. Pausado (s)': tiempo_pausado,
                'Pausas Totales': num_pausas,
                'C. Contexto Vol/Inv': f"{p_data['cambios_contexto_vol']}/{p_data['cambios_contexto_inv']}",
                'Memoria Pico (KB)': p_data['memoria_pico_kb']
            })


    df = pd.DataFrame(registros)
    
    # 2. Formatear las columnas
    df['T. Total (Wall)'] = df['T. Total (Wall)'].apply(lambda x: f"{x:.2f}s")
    df['T. Muerto Kernel'] = df['T. Muerto Kernel'].apply(lambda x: f"{x:.4f}s")
    df['CPU Efectiva (s)'] = df['CPU Efectiva (s)'].apply(lambda x: f"{x:.4f}")
    df['T. Pausado (s)'] = df['T. Pausado (s)'].apply(lambda x: f"{x:.4f}")
    df['Memoria Pico (KB)'] = df['Memoria Pico (KB)'].apply(lambda x: f"{x:,}")

    # 3. Imprimir la tabla
    print("✨ RENDIMIENTO DETALLADO POR CICLO (P1, P2, P3) ✨".center(80))
    print(tabulate(df, headers="keys", tablefmt="fancy_grid", showindex=False))

# ===============================================
# 4. EJECUCIÓN PRINCIPAL
# ===============================================

if __name__ == "__main__":
    
    print("\n--- INICIANDO ANALIZADOR DE MÉTRICAS ---")
    
    # 1. Cargar los datos
    json_data, acumulado_data = cargar_datos_json(ESCENARIO_NUMERO)
    
    if json_data and acumulado_data:
        # Tabla 1: Reporte Acumulado
        generar_tabla_acumulada(acumulado_data)
        
        # Tabla 2: Detalle por Ciclo y Proceso
        generar_tabla_detallada(json_data)
    else:
        print("\n❌ La ejecución ha finalizado debido a errores de carga de archivos.")