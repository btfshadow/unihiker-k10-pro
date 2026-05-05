# Regras do Projeto para Copilot

Estas instrucoes valem para todo o workspace.

## 1) Upload de firmware
- Nao executar upload de firmware por padrao.
- Quando upload for necessario, pedir primeiro para o usuario executar o upload manualmente.
- So executar upload diretamente se o usuario pedir explicitamente.

## 2) Uso de external_packages
- Tratar `external_packages/` como base de referencia tecnica.
- Nao ficar limitado ao que existe em `external_packages/`; sempre buscar otimizar, simplificar e alinhar a implementacao aos objetivos pedidos pelo usuario.
- Implementar evolucoes no codigo do projeto (`unihiker-pro/`) por padrao.
- Se uma alteracao em `external_packages/` for realmente necessaria, pedir aprovacao explicita antes de editar.

## 3) Alinhamento com objetivos
- Priorizar o objetivo atual pedido pelo usuario em vez de reproduzir comportamento legado sem necessidade.
- Sempre explicar rapidamente o que foi alterado e como isso atende ao objetivo.

## 4) Proposito do unihiker-pro
- O `unihiker-pro` deve evoluir para substituir a lib original da placa.
- A implementacao deve cobrir os recursos da placa com duas camadas de uso:
	- interface simplificada para uso rapido;
	- interface completa/avancada para configuracoes detalhadas.
- O codigo deve ser pensado para publicacao em GitHub e uso por terceiros em projetos PlatformIO.

## 5) Engenharia e qualidade
- Priorizar customizacao, desempenho e reducao de acoplamento em cada iteracao.
- Sempre buscar otimizar implementacoes em vez de apenas espelhar a referencia legada.
- Sempre que possivel, adicionar ou expandir testes (preferencia por testes unitarios e smokes focados por modulo).